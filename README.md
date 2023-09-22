# rocket-lab
Solution to Rocket Lab's production automation coding test. 

# Build and Usage
clone or download the repository at https://github.com/jimregiandrew/rocket-lab

The python code assumes python3. I used Python 3.10.12 on Ubuntu 22.04.3 LTS

Install PyQt5 e.g.
```
pip install PyQt5
```
I used a virtual environment and this was the only module I used.

Then, in the repo root directory execute
```
./run.sh
```
This makes the device_simulator c++ program, runs pact.py the Python test (control) program, and runs a single instance of device_simulator.

You can run multiple device_simulators (e.g. in other windows) to get more simulated test devices. Change the DeviceName and MulticastPort for each new test device. (Have a look at run.sh).

# Notes
Not having written socket code before, having written relatively simple python scripts only, and not using PyQt before made this a challenging task for me. But I learned a lot! The pytyhon programs does not do plotting nor define/use pass/fail criteria. I don't think these would be difficult to add but I ran out of time, having spent most of it on the areas that were new to me. I use a single thread event loop for both the python and C++ code. I went down the path of using threads in Python (with slots/signals) but realized that I wouldn't get something going in time. Nonetheless, a single thread is easier to reason about, and  simpler can often be better.
