Video prove's attached https://github.com/dimakarageoz/lesson-5.6/blob/master/video_2026-05-07_18-29-03.mp4

Current code implement DC motor movements according to define steps. 
Steps are corrected with Module KY-040 with 20-steps emitter. One emitter step correspondы to 18 angle of rotation, therefore 10 steps - 180, 20 - 360 and etc.
This approuch we can specify queue of actions that motor should execute, we don't rely on motor power but read the results from emitter

Code example:\
20 - 360 Right,\
-10 - 180 Left,\
100 - 360x5 Right, 5 full revolutions around the axis
