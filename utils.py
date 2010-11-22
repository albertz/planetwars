#!/usr/bin/python

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
	if base:
		copyAttributes(obj, base)
		obj._base = base

def standardRepr(obj):
	str = obj.__class__.__name__ + "("
	attribs = map(lambda attrib: attrib + "=" + repr(getattr(obj, attrib)), objRepresentingArgs(obj))
	str += ", ".join(attribs) + ")"
	return str		

