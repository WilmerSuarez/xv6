# Introduction to xv6

Three features were added to the operating system.

**getdate system-call:** 
```
Read CMOS Real-Time-Clock registers.
```
**setdate system-call:**
```
Program CMOS Real-Time-Clock registers.
```

**Exit status:**

```
Implement process exit status support.
```

## Running the tests

**getdate system-call:**
The test for the getdate system-call simply reads the current cmos rtc registers and sends the values to standard output. 
The time is presented in UTC (Coordinated Universal Time).
- ```./gd```

**setdate system-call:**
The test for the setdate system-call writes a correct value to the cmos rtc registers when no arguments are passed to the program, and then uses the getdate system-call to then read the rtc register values and print them. When any argument is passed to the program, a wrong date and time is sent to modify the cmos rtc registers (it fails) and exits.
- ```./sd```

**Exit status - Process killed**
The exit status is tested and checked by the setdate system-call shown above. It is also tested to see if a process has been killed. If a process is killed the shell prints the word "Killed".
