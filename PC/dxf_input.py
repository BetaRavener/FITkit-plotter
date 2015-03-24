# !/usr/bin/env python
__author__ = 'Ivan'
import ezdxf
import os
import math

class DxfInput:
    def __init__(self, filename):
        if not os.path.exists(filename):
            raise Exception("File does not exists")

        self.dxf = ezdxf.readfile(filename)
        self.modelspace = self.dxf.modelspace()

    def getCommands(self):
        commands = []

        for e in self.modelspace:
            if e.dxftype() == 'LINE':
                command = ('LINE', [])
                command[1].append('%d' % e.dxf.start[0])
                command[1].append('%d' % e.dxf.start[1])
                command[1].append('%d' % e.dxf.end[0])
                command[1].append('%d' % e.dxf.end[1])

            elif e.dxftype() == 'CIRCLE':
                command = ('CIRCLE', [])
                command[1].append('%d' % e.dxf.center[0])
                command[1].append('%d' % e.dxf.center[1])
                command[1].append('%d' % round(e.dxf.radius))

            elif e.dxftype() == 'ARC':
                x = round(e.dxf.center[0] + math.cos(e.dxf.start_angle / 180.0 * math.pi) * e.dxf.radius)
                y = round(e.dxf.center[1] + math.sin(e.dxf.start_angle / 180.0 * math.pi) * e.dxf.radius)

                command = ('MOVE', [])
                command[1].append('%d' % x)
                command[1].append('%d' % y)
                commands.append(command)

                command = ('ARC', [])
                command[1].append('%d' % e.dxf.center[0])
                command[1].append('%d' % e.dxf.center[1])
                command[1].append('%d' % -e.dxf.end_angle)

            else:
                command = None

            if command:
                commands.append(command)

        return commands