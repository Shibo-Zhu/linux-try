Compile tasks
```bash
gcc -O2 -o rt_task1 rt_task1.c -lrt
gcc -O2 -o rt_task2 rt_task2.c -lrt
```

Run tasks with real-time scheduling
```bash
sudo ./rt_task1 
sudo ./rt_task2 
```