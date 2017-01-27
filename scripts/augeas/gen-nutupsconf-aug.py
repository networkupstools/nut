#!/usr/bin/env python
#   Copyright (C) 2010 - Arnaud Quette <arnaud.quette@gmail.com>
#
#   This program is free software; you can redistribute it and/or modify
#   it under the terms of the GNU General Public License as published by
#   the Free Software Foundation; either version 2 of the License, or
#   (at your option) any later version.
#
#   This program is distributed in the hope that it will be useful,
#   but WITHOUT ANY WARRANTY; without even the implied warranty of
#   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#   GNU General Public License for more details.
#
#   You should have received a copy of the GNU General Public License
#   along with this program; if not, write to the Free Software
#   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA

# This program extracts all drivers specific variables, declared
# using 'addvar()' and output a complete ups.conf lens for Augeas

from __future__ import print_function

import sys
import re
import glob
import codecs

# Return a sorted list of unique entries, based on the input 'list'
def sortUnique(list):
	newVarList = []
	prevVar = ''

	# sort the list
	list.sort()

	for curVariable in list:
		if curVariable != prevVar:
			newVarList.append(curVariable)
		prevVar = curVariable

	return newVarList

# Grep for 'string' pattern in the text 'list',
# excluding C/C++ styles comments
# Return the list of matching lines
def grep(string,list):
	matchList = []
	expr = re.compile(string)
	for text in list:
		match = expr.search(text)
		if match != None:
			# Exclude comments
			exprComment = re.compile('(/\*([^*]|(\*+[^*/]))*\*+/)|(//.*)')
			if (exprComment.search(match.string) == None):
				matchList.append(match.string)

	return matchList

if __name__ == '__main__':

	rawCount = 0
	global finalCount
	variableNames = []
	specificVars = ""
	global inLensContent
	global finalLensContent
	Exceptionlist = ['../../drivers/main.c', '../../drivers/skel.c']
	outputFilename = 'nutupsconf.aug.in'
	templateFilename = 'nutupsconf.aug.tpl'
	dirPrefix = ''

	if (len(sys.argv) == 2):
		dirPrefix = sys.argv[1]
		print(dirPrefix)

	# 1/ Extract all specific drivers parameters, in a sorted list with unique entries
	# 1.1/ List all drivers implementation files
	for filename in glob.glob('../../drivers/*.c'):
		# 1.2/ Exclude main.c, which defines addvar() and skel.c (example driver)
		if filename not in Exceptionlist:
			fd = codecs.open(filename, encoding='utf-8')
			# 1.3/ Grep for the "addvar(..." pattern
			matchResults = grep ('.*addvar[\ ]*\(.*(VAR_FLAG|VAR_VALUE)*,.*', fd)

			# 1.4/ Extract variable names
			for line in matchResults:
				row = line.split(',')
				if len(row) >= 2:
					# Absence of quotes indicate that we have a #define
					# Let's grep in .ch related files
					if (row[1].find('"') == -1):
						for defFilename in glob.glob(filename.replace('.c', '.[ch]')):
							defFd = codecs.open(defFilename, encoding='utf-8')
							matchString = '^#define.*' + row[1].replace('"', '').lstrip() + '.*'
							matchResult = grep (matchString, defFd)
							for varDefine in matchResult:
								# Now search for a string
								defRow = re.findall(r'"([^"]*)",?', varDefine)
								if (len(defRow) == 1):
									variableNames.append(defRow[0])
					else:
						# Remove quotes
						variableNames.append(row[1].replace('"', '').lstrip())

	# Filter multiply defined variables
	variableNames = sortUnique(variableNames)

	# Create the formated list of specific variables
	for name in variableNames:
		specificVars += "                 | \"%s\"\n" %(name)

	# 2/ Load the template lens
	tplFd = codecs.open(dirPrefix + templateFilename, encoding='utf-8')

	# 2.1/ Search for the pattern to replace
	outputText = tplFd.read()
	outputText = outputText.replace('@SPECIFIC_DRV_VARS@', specificVars)

	# 3/ Output final lens
	outFd = codecs.open(outputFilename, mode='w', encoding='utf-8')
	outFd.write(outputText)
