import socket
import time
import pickle

fps = 15

f = open('ba.pickle', 'rb')

frames_data = pickle.load(f)



frame_delta = 1 / fps

print("start playing")
s = socket.socket(socket.AddressFamily.AF_INET)
s.connect(("192.168.2.231", 1337))
print(s.recv(7))

start_time = time.time()

for i, frame in enumerate(frames_data):
    s.send(frame)
    next_time = start_time + (frame_delta*(i+1))
    time_to_sleep = next_time - time.time()
    if(time_to_sleep > 0):
        time.sleep(time_to_sleep)
    else:
        print(f'Can\'t keep up: late for {-time_to_sleep} s')
    


s.close()