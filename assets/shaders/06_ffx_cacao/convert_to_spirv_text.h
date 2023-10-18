#!/usr/bin/env python3                                                                                                                                                                                                                                                                                                                                
import sys
import os
import subprocess
import shutil

if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("Usage: " + sys.argv[0] + " root-folder-path")
        sys.exit()
    shutil.rmtree("spv")
    os.mkdir("spv")
    for root, dir, files in os.walk(sys.argv[1]):
          for file in files:
              if not file.endswith(".h"):
                  continue
              with open(file, 'r') as f:
                  lines = f.readlines();
                  startIdx = 0
                  endIdx = 0
                  i = 0
                  for l in lines:
                      if l.startswith("#if"):
                          startIdx = i + 1;
                      elif l.startswith("#endif"):
                          endIdx = i
                          break;
                      i = i + 1
                  spirv = lines[startIdx:endIdx]
                  print(file[:-2])
                  asm_file = file[:-2] + ".spvasm"
                  spv_file = "spv/" + file[:-2] + ".spv"
                  with open(asm_file, 'w') as w:
                      w.write("".join(spirv))
                  subprocess.run(["spirv-as", "-o", spv_file, asm_file])
                  os.remove(asm_file)
                  subprocess.run(["spirv-val", spv_file])
                
	  
