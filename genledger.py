import random, string
import sys, getopt

entrycount = 1000
maxliability = 1000
outfile = 'ledger.txt'
incremental_outfile = 'ledger_incremental.txt'

try:
	opts, args = getopt.getopt(sys.argv[1:],"hn:l:o:p:",[])
except getopt.GetoptError:
	print 'genledger.py -n <entry count> -l <max liability> -o <output file> -p <incremental output file>'
	sys.exit(2)
for opt, arg in opts:
	if opt == '-h':
		print 'genledger.py -n <entry count> -l <max liability> -o <output file> -p <incremental output file>'
		sys.exit()
	elif opt in ("-n"):
		entrycount = int(arg)
	elif opt in ("-l"):
		maxliability = int(arg)
	elif opt in ("-o"):
		outfile = arg
	elif opt in ("-p"):
		incremental_outfile = arg

with open(outfile, "w") as text_file:
	with open(incremental_outfile, "w") as incremental_text_file:
	    text_file.write("{0}\n".format(entrycount * maxliability))
	    incremental_text_file.write("{0}\n".format(entrycount * maxliability))
	    for ii in range(entrycount):
	    	userid = ''.join(random.choice(string.ascii_lowercase + string.digits) for _ in range(8))
	    	text_file.write('{0} {1}\n'.format(userid, random.randint(1,maxliability)))
	    	incremental_text_file.write('{0} {1}\n'.format(userid, random.randint(1,maxliability)))
