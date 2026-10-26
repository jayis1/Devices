#!/usr/bin/env python3
"""Print baseline threshold suggestion from manually observed idle/running RMS samples."""
def suggest(idle_mg, running_mg):
    if not idle_mg or not running_mg: raise ValueError('provide both sample groups')
    return (max(idle_mg)+min(running_mg))/2
if __name__ == '__main__': print('Collect 60s idle and active RMS samples; choose hysteresis around:', suggest([10,20,30],[300,500]))
