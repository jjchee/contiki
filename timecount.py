import re


f = open('16bytesresult.txt', 'r')


p= re.compile(r'(\d{2}):(\d{2}\.\d{3})')

# sum of delays after number of iterations
sum = 0
count = 0

while True:
	#start stamp
	line1 = f.readline()
	if line1 == "": break
	time1 = float(p.findall(line1)[0][0]) * 60 + float(p.findall(line1)[0][1])
	#payload
	f.readline()
	line3 = f.readline()
	if line3 == "": break
	#end stamp
	time3 = float(p.findall(line3)[0][0]) * 60 + float(p.findall(line3)[0][1])
	sum += time3 - time1
	count += 1
	#eof
	if not line3: break


print sum
print count
print sum / count