import os
import sys
import locale
import subprocess
import re

def main():
    (string_list) = sys.argv[1:]
    print(",".join(string_list))

if __name__ == '__main__':
    main()
