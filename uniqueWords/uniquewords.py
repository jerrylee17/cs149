import sys
import threading

args = sys.argv


def getFileWords(file, words):
    try:
        with open(file, 'r') as f:
            contents = f.readlines()
            for line in contents:
                words.extend(filter(lambda x: x != '\n', line.split(' ')))
            words = list(set(words))
    except:
        print(f'{file}: No such file or directory')
    return words


def spawnThread(file, words):
    t = threading.Thread(target=getFileWords, args=(file, words))
    return t


files = args[1:]
words = []
threads = []
for f in files:
    threads.append(spawnThread(f, words))

for t in threads:
    t.start()
for t in threads:
    t.join()

result = len(words)
print(result)
