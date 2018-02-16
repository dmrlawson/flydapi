import dshot

for _ in range(10000):
    dshot.send(48, 19)
    dshot.send(48, 5)
    dshot.send(48, 7)
    dshot.send(48, 20)

for _ in range(20000):
    dshot.send(99, 19)
    dshot.send(99, 5)
    dshot.send(99, 7)
    dshot.send(99, 20)

for _ in range(10000):
    dshot.send(199, 19)
    dshot.send(199, 5)
    dshot.send(199, 7)
    dshot.send(199, 20)
