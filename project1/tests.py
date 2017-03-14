import subprocess
import os

trace_dir = os.path.join(os.getcwd(), 'traces')
test_dir = os.path.join(os.getcwd(), 'tests')

tests = [['./cachesim', '-I', '8:1:1:x', 'traces/medium.txt'],
         ['./cachesim', '-I', '8:4:1:x', 'traces/medium.txt'],
         ['./cachesim', '-I', '8:4:8:R', 'traces/medium.txt'],
         ['./cachesim', '-I', '8:4:8:L', 'traces/medium.txt'],
         ['./cachesim', '-I', '8:1:1:x', '-D', '1:8:1:1:x:T:N', 'traces/medium.txt'],
         ['./cachesim', '-I', '8:1:1:x', '-D', '1:8:4:1:x:T:N', 'traces/medium.txt'],
         ['./cachesim', '-I', '8:1:1:x', '-D', '1:8:4:1:x:T:A', 'traces/medium.txt'],
         ['./cachesim', '-I', '8:1:1:x', '-D', '1:8:4:8:L:T:N', 'traces/medium.txt'],
         ['./cachesim', '-I', '8:1:1:x', '-D', '1:8:4:8:L:T:A', 'traces/medium.txt'],
         ['./cachesim', '-I', '8:1:1:x', '-D', '1:8:4:8:L:B:A', 'traces/medium.txt'],
         ['./cachesim', '-I', '8:1:1:x', '-D', '1:32:1:32:L:B:A', 'traces/medium.txt'],
         ['./cachesim', '-I', '2048:4:4:L', '-D', '1:2048:4:4:L:B:A', 
                                            '-D', '2:32768:4:2:L:B:A', 
                                            '-D', '3:524288:4:2:L:B:A', 'traces/gzip.txt']]

for t in range(len(tests)):
    print("*****TEST*****", t+1)
    if t == 2:
        print("Skipping random replacement test")
        continue
    output = subprocess.check_output(tests[t])
    output = [o.split() for o in str(output, encoding='utf-8').splitlines()]
    with open(os.path.join('tests','test'+str(t+1).zfill(2))) as true_output:
        for i, line in enumerate(true_output):
            line = line.strip().split()
            if line == "L1 D-Cache statistics:".split():
                while len(output)>1 and output[i] != "L1 D-Cache statistics:".split():
                    output.pop(i)
            print(line)
            if(output[i] == line):
                #print("CORRECT")
                pass
            else:
                print("INCORRECT on test", t+1)
                print("Program output:\n", output[i])
                exit()
                                                   
