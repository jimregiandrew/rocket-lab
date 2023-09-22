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

#global program_finished
program_finished = False
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

class SocketWorker(QRunnable):

    def __init__(self, *args, **kwargs):
        super(SocketWorker, self).__init__()
        self.server_socket = setup_multicast_socket(MCAST_GROUP, MCAST_PORT)

    @pyqtSlot()
    def run(self):
        while not program_finished:
            print("SW: socket thread: program_finished=",program_finished)
            timeout = 1.0
            r, _, _ = select.select([self.server_socket], [], [], timeout)
            if r:
                message, address = self.server_socket.recvfrom(1024)
                message = message.decode().upper().strip()
                print("SW: got message " + message + " from", address)
                self.server_socket.sendto(bytes(message,'utf-8')+bytes("-reply", 'utf-8'), address)
        print("SW: Finished socket thread")

class WorkerSignals(QObject):
    '''
    Defines the signals available from a running worker thread.

    Supported signals are:

    finished
        No data

    error
        tuple (exctype, value, traceback.format_exc() )

    result
        object data returned from processing, anything

    progress
        int indicating % progress

    '''
    finished = pyqtSignal()
    error = pyqtSignal(tuple)
    result = pyqtSignal(object)
    progress = pyqtSignal(int)


class Worker(QRunnable):
    '''
    Worker thread

    Inherits from QRunnable to handler worker thread setup, signals and wrap-up.

    :param callback: The function callback to run on this worker thread. Supplied args and
                     kwargs will be passed through to the runner.
    :type callback: function
    :param args: Arguments to pass to the callback function
    :param kwargs: Keywords to pass to the callback function

    '''

    def __init__(self, fn, *args, **kwargs):
        super(Worker, self).__init__()

        # Store constructor arguments (re-used for processing)
        self.fn = fn
        self.args = args
        self.kwargs = kwargs
        self.signals = WorkerSignals()

        # Add the callback to our kwargs
        self.kwargs['progress_callback'] = self.signals.progress

    @pyqtSlot()
    def run(self):
        '''
        Initialise the runner function with passed args, kwargs.
        '''

        # Retrieve args/kwargs here; and fire processing using them
        try:
            result = self.fn(*self.args, **self.kwargs)
        except:
            traceback.print_exc()
            exctype, value = sys.exc_info()[:2]
            self.signals.error.emit((exctype, value, traceback.format_exc()))
        else:
            print("HERE!!")
            self.signals.result.emit("DOOM")  # Return the result of the processing
        finally:
            self.signals.finished.emit()  # Done



class MainWindow(QMainWindow):


    def __init__(self, *args, **kwargs):
        super(MainWindow, self).__init__(*args, **kwargs)

        self.counter = 0
        
        # GUI
        layout = QVBoxLayout()
        dd_widget = QPushButton("Discover devices")
        dd_widget.pressed.connect(self.discover_devices)
        layout.addWidget(dd_widget)
        sd_widget = QPushButton("Select device")
        sd_widget.pressed.connect(self.select_device)
        layout.addWidget(sd_widget)
        start_test_widget = QPushButton("Start test")
        start_test_widget.pressed.connect(self.start_test)
        layout.addWidget(start_test_widget)
        self.dtd_widget = QLineEdit()
        #self.dtd_widget.pressed.connect(self.define_test_duration)
        duration_row = QFormLayout()
        duration_row.addRow("Duration", self.dtd_widget)
        layout.addLayout(duration_row)
        stop_widget = QPushButton("Stop test")
        stop_widget.pressed.connect(self.stop_test)
        layout.addWidget(stop_widget)

        w = QWidget()
        w.setLayout(layout)
        self.setCentralWidget(w)
        self.show()

        # Threads
        self.threadpool = QThreadPool()
        print("Multithreading with maximum %d threads" % self.threadpool.maxThreadCount())

        # Timer for incrementing counter
        self.timer = QTimer()
        self.timer.setInterval(1000)
        self.timer.timeout.connect(self.recurring_timer)
        self.timer.start()

        # Socket thread
        # sw = SocketWorker() # create Worker object that calls self.execute_this_fn (worker.fn)
        # Start sw (thread)
        # self.threadpool.start(sw)
        self.timer=QTimer()
        self.timer.timeout.connect(self.check_socket)
        self.timer.start(1000)
        
    def check_socket(self):
        pass

    def progress_fn(self, n):
        print("%d%% done" % n)

    def execute_this_fn(self, progress_callback):
        for n in range(0, 5):
            time.sleep(1)
            percent = n * 100 / 4.0
            print("percent=", percent)
            progress_callback.emit(int(percent))

        return "Done."

    def print_output(self, s):
        print("result", s)

    def thread_complete(self):
        print("THREAD COMPLETE!")

    def discover_devices(self):
        MESSAGE = "ID;"
        print("Sening message:", MESSAGE)
        sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM) # UDP
        sock.settimeout(0.01) # Can't be 0
        sock.sendto(bytes(MESSAGE, "utf-8"), (MCAST_GROUP, MCAST_PORT))        # Pass the function to execute
        # Look for responses from all recipients
        while True:
            print("waiting to receive")
            try:
                data, server = sock.recvfrom(1024)
            except socket.timeout:
                print("timed out, no more responses")
                break
            else:
                print("received ",data, " from ", server)

        
        #worker = Worker(self.execute_this_fn) # create Worker object that calls self.execute_this_fn (worker.fn)
        #worker.signals.result.connect(self.print_output)
        #worker.signals.finished.connect(self.thread_complete)
        #Worker.signals.progress.connect(self.progress_fn)

        # Start worker (thread)
        #self.threadpool.start(worker)

    def select_device(self):
        print("Selecting device")

    def define_test_duration(self):
        print(self.dtd_widget.text())

    def start_test(self):
        durstr = self.dtd_widget.text()
        if (not durstr.isdigit()):
            raise_error(durstr + " is not an integer. Please enter an integer duration in seconds")
            return
        print("starting test, duration=", self.dtd_widget.text())
        sock = setup_multicast_socket(MCAST_GROUP, MCAST_PORT)
        sock.settimeout(3.0)
        sock.sendto(bytes("TEST;CMD=START;DURATION=" + durstr + "RATE=1000", "utf-8"), (MCAST_GROUP, MCAST_PORT))
        # Look for responses from all recipients
        while True:
            print("2. waiting to receive")
            try:
                data, server = sock.recvfrom(1024)
            except socket.timeout:
                print("2. timed out, no more responses")
                break
            else:
                print("2. received ",data, " from ", server)

    def stop_test(self):
        print("Stop test")

    def exit_program(self):
        global program_finished
        program_finished = True
        print("program_finished=",program_finished)

    def recurring_timer(self):
        self.counter +=1
        #self.l.setText("Counter: %d" % self.counter)


app = QApplication([])
window = MainWindow()
app.aboutToQuit.connect(window.exit_program)
app.exec_()

