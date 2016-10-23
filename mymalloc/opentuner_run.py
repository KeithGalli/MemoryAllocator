#!/usr/bin/python2.7
#
import logging
import os
import threading
import opentuner
from opentuner import ConfigurationManipulator
from opentuner import MeasurementInterface
from opentuner import Result
from opentuner.measurement.inputmanager import FixedInputManager
from opentuner.search.objective import MaximizeAccuracy
from opentuner.search.manipulator import PowerOfTwoParameter
import opentuner_params


# Convert numeric strings to the appropriate type.
def try_num(s):
    try:
        return int(s)
    except ValueError:
        try:
            return float(s)
        except ValueError:
            return s


# Parse stdout from mdriver when executed locally.
def parse_stdout(stdout):
    result = {}
    for line in stdout.splitlines():
        line_split = line.split(':', 1)
        if len(line_split) == 2:
            key, value = line_split
            result[key] = try_num(value)
    return result


# Parse stdout from mdriver when executed on AWS worker machines.
def parse_awsrun_stdout(stdout):
    result = {}
    for line in stdout.splitlines():
        if line.startswith('Submitting Job:') or line.startswith('Waiting for job to finish'):
            continue
        line_split = line.split(':', 1)
        if len(line_split) == 2:
            key, value = line_split
            result[key] = try_num(value)
    return result
            

class MdriverTuner(MeasurementInterface):
  # Find configuration with the best accuracy/perfidx.
  best_accuracy = 0

  # Remember best parameters through commands.
  best_make_cmd = best_bin_cmd = ''

  # Lock that protects the previous fields.
  lock = threading.Lock()

  def __init__(self, args):
    # Ensure either a file or directory is specified.
    assert args.trace_file != None or args.trace_dir != None

    # Maximize perfidx, and cache the fixed inputs.
    super(MdriverTuner, self).__init__(
      args,
      objective=MaximizeAccuracy(),
      input_manager=FixedInputManager())

  def manipulator(self):
    """
    Define the search space by creating a
    ConfigurationManipulator
    """
    
    # Use the params as defined in the external file.
    return opentuner_params.mdriver_manipulator

  def save_final_config(self, configuration):
    """
    Called at the end of tuning
    """
    if self.args.trace_file is not None:
        print 'trace_file: ' + self.args.trace_file
    else:
        print 'trace_dir: ' + self.args.trace_dir
    print 'make_cmd: ' + self.best_make_cmd
    print 'bin_cmd: ' + self.best_bin_cmd
    print 'perfidx: ' + str(self.best_accuracy)
    print

  def run(self, desired_result, input, limit):
    """
    Compile and run a given configuration then
    return performance
    """
    accuracy = 0
    time = 0
    cfg = desired_result.configuration.data

    # Tell the user whether the command is running locally.
    awsrun = self.args.awsrun
    if awsrun:
        print "Running on AWS worker machines..."
    else:
        print "Running locally..."

    # Generate the params to pass to compiler from the requested configuration.
    gcc_params = ''
    for key, value in cfg.iteritems():
      gcc_params += '-D {0}={1} '.format(key, value)
    make_cmd = ''

    # Generate the make command.
    if awsrun:
        make_cmd = 'make partial_clean mdriver GETTIME=1 DEBUG=0 PARAMS="{0}"'.format(gcc_params)
    else:
        make_cmd = 'make partial_clean mdriver DEBUG=0 PARAMS="{0}"'.format(gcc_params)

    # Make the executable, on failure return 0 perfidx.
    compile_result = self.call_program(make_cmd, limit = self.args.make_timeout)
    if compile_result['returncode'] != 0:
      return Result(accuracy=accuracy, time=time)

    # Generate the mdriver option for traces.
    trace_params = ''
    if self.args.trace_file is not None:
        trace_params = '-f ' + self.args.trace_file
    else:
        trace_params = '-t ' + self.args.trace_dir

    # Generate the mdriver command.
    bin_cmd = ''
    if awsrun:
        bin_cmd = 'awsrun ./mdriver -g ' + trace_params
    else:
        bin_cmd = './mdriver -g ' + trace_params

    # Run the command.
    run_result = self.call_program(bin_cmd, limit = self.args.command_timeout)

    if awsrun:
        # Parse the output.
        result = parse_awsrun_stdout(run_result['stdout'])

        # Reject if the command never gives statistics.
        if len(result) < 3 or 'runtime' not in result:
            return Result(accuracy=accuracy, time=time)

        # Fetch the statistics.
        time += result['runtime']
        accuracy = result.get('perfidx', accuracy)

        # Update the best results if needed.
        with self.lock:
            if accuracy > self.best_accuracy:
                self.best_accuracy = accuracy
                self.best_make_cmd = make_cmd
                self.best_bin_cmd = bin_cmd

        return Result(accuracy=accuracy, time=time)
    else:
        # Get the run time.
        time += run_result['time']

        # Reject if the command times out or fails.
        if run_result['timeout'] or run_result['returncode'] != 0:
            return Result(accuracy=accuracy, time=time)

        # Parse the output.
        result = parse_stdout(run_result['stdout'])

        # Get the perfidx if it exists.
        accuracy = result.get('perfidx', accuracy)

        # Update the best results if needed.
        with self.lock:
            if accuracy > self.best_accuracy:
                self.best_accuracy = accuracy
                self.best_make_cmd = make_cmd
                self.best_bin_cmd = bin_cmd

        return Result(accuracy=accuracy, time=time)
        

if __name__ == '__main__':
  logging.basicConfig(level=logging.ERROR)
  argparser = opentuner.default_argparser()
  file_group = argparser.add_mutually_exclusive_group()
  file_group.add_argument('--trace-dir', default = 'traces',
                          help = 'trace directory to pass to mdriver')
  file_group.add_argument('--trace-file', default = None,
                          help = 'trace file to pass to mdriver')
  argparser.add_argument('--command-timeout', type = int, default = 60,
                         help = 'timeout for an mdriver invocation in seconds')
  argparser.add_argument('--make-timeout', type = int, default = 20,
                         help = 'timeout for a make invocation in seconds')
  argparser.add_argument('--awsrun', action = 'store_true',
                         help = 'run on AWS worker machines instead of local')
  args = argparser.parse_args()
  MdriverTuner.main(args)
