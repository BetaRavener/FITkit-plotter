# !/usr/bin/env python
import signal
import argparse
from commander import FitKitClient, print_error

# noinspection PyUnusedLocal
def sigTermHandler(signum, frame):
    fitKitClient.queueClose()


class ArgParseException(Exception):
    pass


class ThrowingArgumentParser(argparse.ArgumentParser):
    def error(self, message):
        raise ArgParseException(message)

def processArguments(args):
    mode = None
    if args['l']:
        mode = FitKitClient.listeningMode
    if args['w']:
        if mode:
            raise Exception('Argument error: only one mode may be selected.')
        else:
            mode = FitKitClient.writingMode
    if args['f']:
        if mode:
            raise Exception('Argument error: only one mode may be selected.')
        else:
            mode = FitKitClient.fullMode

    if not mode:
        mode = FitKitClient.fullMode

    return mode


def mainProcess(fitKitClient):
    parser = ThrowingArgumentParser(description='Simple IRC client.', add_help=False)
    parser.add_argument('-l', action='store_true')
    parser.add_argument('-w', action='store_true')
    parser.add_argument('-f', action='store_true')

    try:
        args = parser.parse_args()
    except ArgParseException, e:
        print_error('Argument error: ' + e.message)
        return

    try:
        mode = processArguments(vars(args))
    except Exception, e:
        print_error(e.message)
        return

    try:
        fitKitClient.run(mode)
    except Exception, e:
        pass
    finally:
        fitKitClient.close()


signal.signal(signal.SIGTERM, sigTermHandler)
signal.signal(signal.SIGINT, sigTermHandler)
fitKitClient = FitKitClient()

mainProcess(fitKitClient)