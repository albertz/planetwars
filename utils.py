#!/usr/bin/python

import random

def objRepresentingArgs(obj):
	for attrib in obj.__dict__:
		if not attrib.startswith("_"):
			yield attrib

def copyAttributes(dst, src):
	for attrib in objRepresentingArgs(src):
		setattr(dst, attrib, getattr(src, attrib))

def initFromKWArgs(dst, args):
	for attrib in args:
		setattr(dst, attrib, args[attrib])
	

def handleBase(obj, base):
	if base: copyAttributes(obj, base)

def handleKWArgs(obj, kwargs):
	for arg,value in kwargs.iteritems():
		setattr(obj, arg, value)


def standardRepr(obj):
	str = obj.__class__.__name__ + "("
	attribs = map(lambda attrib: attrib + "=" + repr(getattr(obj, attrib)), objRepresentingArgs(obj))
	str += ", ".join(attribs) + ")"
	return str		



def call_exposed(func):
	return lambda args: func(*args)


def isempty(iterable):
	try:
		iterable.__iter__().next()
		return False
	except StopIteration:
		return True


def multidict(iterable, innertype = list, addfunc = list.append):
	d = {}
	for key,value in iterable:
		if not key in d:
			d[key] = innertype()
		addfunc(d[key], value)
	return d

def takeBestShuffledList(l, evalFunc, n):
	l = list(l)
	bestList = None
	bestValue = None
	for i in xrange(n):
		random.shuffle(l)
		value = evalFunc(l)
		if bestList is None or value > bestValue:
			bestList = list(l)
			bestValue = value
	return bestList, bestValue

