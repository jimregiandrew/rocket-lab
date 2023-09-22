from PyQt5.QtGui import *
from PyQt5.QtWidgets import *
from PyQt5.QtCore import *

import time
import traceback, sys

import random
import socket
import struct
import select

# https://stackoverflow.com/questions/27893804/udp-client-server-socket-in-python
# https://stackoverflow.com/questions/603852/how-do-you-udp-multicast-in-python
# https://stackoverflow.com/questions/10692956/what-does-it-mean-to-bind-a-multicast-udp-socket

#global PROGRAM_FINISHED
PROGRAM_FINISHED = False
MCAST_GROUP = '224.3.11.15'
MCAST_PORT = 31115

def setup_multicast_socket(mcast_group, mcast_port):
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM, socket.IPPROTO_UDP)
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    mreq = struct.pack("4sl", socket.inet_aton(mcast_group), socket.INADDR_ANY)
    sock.setsockopt(socket.IPPROTO_IP, socket.IP_ADD_MEMBERSHIP, mreq)
    #server_socket.setsockopt(socket.SOL_IP, socket.IP_MULTICAST_IF, socket.inet_aton(mcast_group))
    sock.bind(('', mcast_port))
    return sock

def raise_error(error_str):
    msg = QMessageBox()
    msg.setIcon(QMessageBox.Critical)
    msg.setText("Error")
    msg.setInformativeText(error_str)
    msg.setWindowTitle("Error")
    msg.exec_()


class MainWindow(QMainWindow):

    def __init__(self, *args, **kwargs):
        super(MainWindow, self).__init__(*args, **kwargs)

        self.counter = 0
        
        # GUI
        layout = QVBoxLayout()
        dd_widget = QPushButton("Discover devices")
        dd_widget.pressed.connect(self.discover_devices)
        layout.addWidget(dd_widget)
        self.device_list_widget = QListWidget()
        layout.addWidget(self.device_list_widget)
        self.dtd_widget = QLineEdit()
        #self.dtd_widget.pressed.connect(self.define_test_duration)
        duration_row = QFormLayout()
        duration_row.addRow("Duration", self.dtd_widget)
        layout.addLayout(duration_row)
        start_test_widget = QPushButton("Start test")
        start_test_widget.pressed.connect(self.start_test)
        layout.addWidget(start_test_widget)
        stop_widget = QPushButton("Stop test")
        stop_widget.pressed.connect(self.stop_test)
        layout.addWidget(stop_widget)

        w = QWidget()
        w.setLayout(layout)
        self.setCentralWidget(w)
        self.show()

        self.timer=QTimer()
        self.timer.timeout.connect(self.check_socket)
        self.timer.start(100)
        
        self.sock = setup_multicast_socket(MCAST_GROUP, MCAST_PORT)
        self.devices = []
        
    def check_socket(self):
        timeout = 0.01
        r, _, _ = select.select([self.sock], [], [], timeout)
        if r:
            try:
                data, address = self.sock.recvfrom(1024)
            except socket.timeout:
                print("check_socket: timed out, no more responses")
            else:
                print("check_socket: received ",data, " from ", address)

    def discover_devices(self):
        message = "ID;"
        print("Sending message:", message)
        self.sock.settimeout(0.01) # Can't be 0
        self.sock.sendto(bytes(message, "utf-8"), (MCAST_GROUP, MCAST_PORT))
        self.device_list_widget.clear()
        self.devices.clear()
        # Look for responses from all recipients
        while True:
            print("waiting to receive")
            try:
                data, address = self.sock.recvfrom(1024)
            except socket.timeout:
                print("timed out, no more responses")
                break
            else:
                print("received ",data, " from ", address)
                self.devices.append(address)
                self.device_list_widget.insertItem(self.device_list_widget.count(), ",".join(map(str,address)))

    def define_test_duration(self):
        print(self.dtd_widget.text())

    def start_test(self):
        durstr = self.dtd_widget.text()
        if (not durstr.isdigit()):
            raise_error("Duration is not an integer. Please enter an integer duration in seconds")
            return
        devices_idx = self.device_list_widget.currentRow()
        if (not devices_idx in range(self.device_list_widget.count())):
            raise_error("Please select device in list below 'Discover devices'. (You may need to press 'Discover devices' button again.)")
            return
        print("Starting test, device=", self.device_list_widget.currentItem().text(), ", duration=", self.dtd_widget.text())
        self.sock.sendto(bytes("TEST;CMD=START;DURATION=" + durstr + "RATE=1000", "utf-8"), self.devices[devices_idx])

    def stop_test(self):
        print("Stopping test")
        devices_idx = self.device_list_widget.currentRow()
        if (not devices_idx in range(len(self.devices))):
            raise_error("Please select device in list below 'Discover devices'. (You may need to press 'Discover devices' button again.)")
            return
        self.sock.sendto(bytes("TEST;CMD=STOP;", "utf-8"), self.devices[devices_idx])

    def exit_program(self):
        global PROGRAM_FINISHED
        PROGRAM_FINISHED = True
        print("PROGRAM_FINISHED=",PROGRAM_FINISHED)

    def recurring_timer(self):
        self.counter +=1
        #self.l.setText("Counter: %d" % self.counter)


app = QApplication([])
window = MainWindow()
app.aboutToQuit.connect(window.exit_program)
app.exec_()

