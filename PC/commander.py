# !/usr/bin/env python
__author__ = 'Ivan'

import threading
import sys
import Queue
import time

import fitkit.fitkit as fitkit
from dxf_input import DxfInput

def print_error(message):
    sys.stderr.write(message + '\n')


class Message:
    def __init__(self, command, params=None):
        self.command = command
        self.params = params

    def __str__(self):
        res = ''

        # Append command
        res += self.command

        if self.params:
            # Append parameters
            for param in self.params:
                res += ' ' + param

        res += '\r\n'
        return res


class InitializedReply:
    def __init__(self):
        pass


class DrawingStartedReply:
    def __init__(self):
        pass


class DrawingFinishedReply:
    def __init__(self):
        pass


class ComplexDrawingFinishedReply:
    def __init__(self):
        pass

class DebugReply:
    def __init__(self, debug_text):
        self.debugText = debug_text

    def __str__(self):
        res = 'Debug: '
        if self.debugText:
            res += ': ' + self.debugText
        res += '.\n'
        return res


class QuitReply:
    def __init__(self):
        pass

    def __str__(self):
        # # Format
        # return 'Server closed connection.'
        return ''


class ErrorReply:
    def __init__(self, error_text):
        self.errorText = error_text

    def __str__(self):
        res = 'Server replied with error'
        if self.errorText:
            res += ': ' + self.errorText
        res += '.\n'

        return res


class QuitNotification:
    def __init__(self):
        pass


class ReplyNotification:
    def __init__(self, reply):
        self.reply = reply


class MessageNotifiaction:
    def __init__(self, msgStr, id):
        self.id = id
        self.messageString = msgStr


class InitializeCommand:
    def __init__(self):
        pass


class DrawingCommand:
    def __init__(self):
        pass


class ComplexDrawingCommand:
    def __init__(self):
        self.started = False


class QuitCommand:
    def __init__(self):
        pass


class FitKitClient:
    writingMode = 1
    listeningMode = 2
    fullMode = 3

    def __init__(self):
        self.mode = 0
        self.replyTimeout = 10
        self.drawingTimeout = 1000
        self.issuedCommand = None

        self.mainQueue = Queue.Queue()

        self.inputQueue = Queue.Queue()
        self.inputThread = threading.Thread(None, self.input)
        self.inputThread.daemon = True

        self.sendQueue = Queue.Queue()
        self.sendThread = threading.Thread(None, self.send)

        self.listenQueue = Queue.Queue()
        self.listeningThread = threading.Thread(None, self.listen)

        self.comChannel = None

    def run(self, mode):
        self.mode = mode

        manager = fitkit.DeviceMgr()
        ret = manager.discover()
        if ret < 1:
            raise Exception("No device")

        device = manager.acquire()
        assert isinstance(device, fitkit.Device)
        if device is None:
            raise Exception("No device")

        print "VID: %04X  PID: %04X  SN: %s DECR: %s" % (device.vid(), device.pid(),
                                                         device['b'].serial(), device['b'].product())

        ch = device['b']
        assert isinstance(ch, fitkit.IOChannel)
        ret = ch.open()
        if ret != fitkit.IOChannel.Ok:
            raise Exception("Can't open")

        ch.resetMCU()

        self.comChannel = ch

        self.queueToSend(Message(''), InitializeCommand())

        try:
            self.inputThread.start()
            self.sendThread.start()
            self.listeningThread.start()
        except Exception, e:
            print_error('Could not start all threads.')
            return

        # Application is running while there is any input
        # and while the listening thread is alive
        while True:
            inputItem = None
            try:
                inputItem = self.mainQueue.get(True, 0.1)
            except Queue.Empty, e:
                pass

            if isinstance(inputItem, QuitNotification):
                break

    def input(self):
        while True:
            try:
                inputStr = raw_input()
            except (KeyboardInterrupt, EOFError):
                self.mainQueue.put(QuitNotification())
                return

            while True:
                try:
                    queueItem = self.inputQueue.get_nowait()
                    if isinstance(queueItem, QuitNotification):
                        return
                except Queue.Empty:
                    break

            if (self.mode == FitKitClient.writingMode) or (self.mode == FitKitClient.fullMode):
                if not self.processInput(inputStr):
                    # Quit command was issued
                    self.mainQueue.put(QuitNotification())
                    return
            else:
                print_error("Shhhh! You're listening.")

    def unsplit(self, parts):
        return ' '.join(parts)

    def printReply(self, reply):
        replyStr = str(reply)

        if replyStr:
            if isinstance(reply, ErrorReply):
                print_error(replyStr)
            else:
                print replyStr

    def processInput(self, text):
        if len(text) > 0:
            parts = text.split(' ')
            # Remove slash (already checked)
            command = parts[0]
            if command == 'move' and len(parts) == 3:
                self.queueToSend(Message('MOVE', parts[1:]), DrawingCommand())
            elif command == 'line' and len(parts) == 5:
                self.queueToSend(Message('LINE', parts[1:]), DrawingCommand())
            elif command == 'arc' and len(parts) == 4:
                self.queueToSend(Message('ARC', parts[1:]), DrawingCommand())
            elif command == 'circle' and len(parts) == 4:
                self.queueToSend(Message('CIRCLE', parts[1:]), DrawingCommand())
            elif command == 'demo' and len(parts) == 1:
                self.queueToSend(Message('DEMO', parts[1:]), ComplexDrawingCommand())
            elif command == 'hilbert' and len(parts) == 2:
                self.queueToSend(Message('HILBERT', parts[1:]), ComplexDrawingCommand())
            elif command == 'read' and len(parts) == 2:
                try:
                    dxfInput = DxfInput(parts[1])
                    for id, params in dxfInput.getCommands():
                        self.queueToSend(Message(id, params), DrawingCommand())
                except:
                    print_error('Error drawing the file')
            elif command == 'quit' and len(parts) == 1:
                return False
            else:
                print_error('Invalid command.')

        return True

    def queueClose(self):
        self.mainQueue.put(QuitNotification())

    def close(self):
        self.queueToSend(Message('QUIT'), QuitCommand())

        self.inputQueue.put(QuitNotification())
        if self.sendThread.isAlive():
            self.sendQueue.put(QuitNotification())
            self.sendThread.join()
        if self.listeningThread.isAlive():
            self.listenQueue.put(QuitNotification())
            self.listeningThread.join()

        # reset MCU
        self.comChannel.resetMcu()
        self.comChannel.setRts(False)
        self.comChannel.setDtr(False)

        self.comChannel.close()

    def queueToSend(self, msg, id=None):
        if isinstance(msg, Message):
            msgStr = str(msg)
        else:
            msgStr = msg + '\r\n'

        self.sendQueue.put(MessageNotifiaction(msgStr, id))

    def checkCommand(self, command):
        return True

    def send(self):
        commands = []
        awaitReply = False
        timeout = 0
        while True:
            if awaitReply:
                start = time.time()
                try:
                    queueItem = self.sendQueue.get(True, timeout)
                except Queue.Empty:
                    print_error('Server did not reply in time.')
                    self.mainQueue.put(QuitNotification())
                    return

                # Subtract the seconds passed
                timeout -= (time.time() - start)
            else:
                queueItem = self.sendQueue.get(True)

            if isinstance(queueItem, QuitNotification):
                return

            elif isinstance(queueItem, ReplyNotification):
                if isinstance(queueItem.reply, DrawingStartedReply):
                    if isinstance(self.issuedCommand, DrawingCommand):
                        print ("Drawing has started")
                        timeout = self.drawingTimeout

                    if isinstance(self.issuedCommand, ComplexDrawingCommand) and not self.issuedCommand.started:
                        if not self.issuedCommand.started:
                            print ("Complex drawing has started")
                            self.issuedCommand.started = True

                        timeout = self.drawingTimeout

                elif isinstance(queueItem.reply, DrawingFinishedReply):
                    if isinstance(self.issuedCommand, DrawingCommand):
                        print ("Drawing has finished")
                        self.issuedCommand = None
                        awaitReply = False

                elif isinstance(queueItem.reply, ComplexDrawingFinishedReply):
                    if isinstance(self.issuedCommand, ComplexDrawingCommand):
                        print ("Complex drawing has finished")
                        self.issuedCommand = None
                        awaitReply = False

                elif isinstance(queueItem.reply, InitializedReply):
                    if isinstance(self.issuedCommand, InitializeCommand):
                        print ("FITkit initialized")
                        self.issuedCommand = None
                        awaitReply = False

                elif isinstance(queueItem.reply, QuitReply):
                    self.mainQueue.put(QuitNotification)
                    return

            elif isinstance(queueItem, MessageNotifiaction):
                commands.append(queueItem)

            # Do not send next command if still waiting for reply
            while commands and not awaitReply:
                command = commands.pop(0)
                assert isinstance(command, MessageNotifiaction)
                if not self.checkCommand(command):
                    continue
                commandStr = command.messageString
                if command.id:
                    self.issuedCommand = command.id
                    awaitReply = True
                    if isinstance(self.issuedCommand, InitializeCommand):
                        timeout = self.drawingTimeout
                    else:
                        timeout = self.replyTimeout
                else:
                    awaitReply = False

                try:
                    for ch in commandStr:
                        self.comChannel.write(ch, 1)

                except:
                    print_error('Error while writing to FITkit')
                    self.mainQueue.put(QuitNotification())
                    return


    def listen(self):
        messages = []
        textBuffer = ''
        while True:
            try:
                queueItem = self.inputQueue.get_nowait()
                if isinstance(queueItem, QuitNotification):
                    return
            except Queue.Empty:
                pass

            try:
                data = self.comChannel.read(1, 200)
                if data is not None:
                    textBuffer += data

            except RuntimeError, e: #read terminated
                print "Exception",e
                self.mainQueue.put(QuitNotification())
                return

            try:
                # Parse received data
                lines = textBuffer.split('\r\n')
                for line in lines[:-1]:
                    currentPart = 0

                    line = line.strip(' ')

                    # Skip empty lines
                    if not line:
                        continue

                    # Ignore line that repeats input
                    if line[0] == '>':
                        line = line[1:]

                    parts = line.split(' ')

                    # Extract command text / code
                    command = ''
                    if parts[currentPart] and parts[currentPart][0] == '!':
                        command = parts[currentPart][1:]
                        currentPart += 1

                    # Extract parameters (if any)
                    params = []
                    while currentPart < len(parts):
                        # Work with non empty parts
                        if parts[currentPart]:
                            # Watch for ':' that will mark trailing parameter
                            if parts[currentPart][0] == ':':
                                # Remove colon ':' from first part
                                trailingParam = parts[currentPart][1:]
                                currentPart += 1

                                # Now we need to rebuild the string from rest of parts
                                if currentPart < len(parts):
                                    trailingParam += ' ' + self.unsplit(parts[currentPart:])

                                params.append(trailingParam)
                                break

                            params.append(parts[currentPart])
                        currentPart += 1

                    messages.append(Message(command, params))

                # Save the rest of unprocessed data
                textBuffer = lines[-1]

                # Now start processing
                while len(messages) > 0:
                    if not self.process(messages.pop(0)):
                        return

            except Exception, e:
                print_error('Error occured while listening: ' + e.message)
                self.mainQueue.put(QuitNotification())
                return

    def setReplyReady(self, reply):
        self.sendQueue.put(ReplyNotification(reply))

    def process(self, msg):
        assert isinstance(msg, Message)
        if msg.command:
            if msg.command == 'INITIALIZED':
                self.setReplyReady(InitializedReply())

            if msg.command == 'STARTED':
                self.setReplyReady(DrawingStartedReply())

            if msg.command == 'FINISHED':
                self.setReplyReady(DrawingFinishedReply())

            if msg.command == 'COMPLEX_FINISHED':
                self.setReplyReady(ComplexDrawingFinishedReply())

            if msg.command == 'ERROR':
                self.setReplyReady(ErrorReply())

            if msg.command == 'QUIT':
                self.setReplyReady(QuitReply())
                return False

        else:
            print('%s' % self.unsplit(msg.params))

        return True