from __future__ import print_function
import sys

class Logger(object):

  class Level(object):
    NoOutput, Error, Info = range(3)

  class Color(object):
    Default, Blue, Gray, Purple, Red = range(5)

    @staticmethod
    def terminalCode(color, out=sys.stdout):
      if not out.isatty():
        return ''
      elif color == Logger.Color.Blue:
        return '\033[94m'
      elif color == Logger.Color.Gray:
        return '\033[37m'
      elif color == Logger.Color.Purple:
        return '\033[95m'
      elif color == Logger.Color.Red:
        return '\033[91m'
      else:
        return '\033[0m'

  Verbosity = Level.Info

  @staticmethod
  def log(text, level=Level.Info, color=Color.Default, newLine=True, out=sys.stdout):
    if level <= Logger.Verbosity:
      text = Logger.Color.terminalCode(color, out) + text + \
             Logger.Color.terminalCode(Logger.Color.Default, out)
      if newLine:
        print(text, file=out)
      else:
        print(text, end="", file=out)
      out.flush()

  @staticmethod
  def fail(msg, file=None, line=-1):
    location = ""
    if file:
      location += file + ":"
    if line > 0:
      location += str(line) + ":"
    if location:
      location += " "

    Logger.log(location, Logger.Level.Error, color=Logger.Color.Gray, newLine=False, out=sys.stderr)
    Logger.log("error: ", Logger.Level.Error, color=Logger.Color.Red, newLine=False, out=sys.stderr)
    Logger.log(msg, Logger.Level.Error, out=sys.stderr)
    sys.exit(1)

  @staticmethod
  def startTest(name):
    Logger.log("TEST ", color=Logger.Color.Purple, newLine=False)
    Logger.log(name + "... ", newLine=False)

  @staticmethod
  def testPassed():
    Logger.log("PASS", color=Logger.Color.Blue)

  @staticmethod
  def testFailed(msg, file=None, line=-1):
    Logger.log("FAIL", color=Logger.Color.Red)
    Logger.fail(msg, file, line)
