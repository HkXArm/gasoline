#!/usr/bin/python
import os
import time
import shutil
import pynbody as pyn
import numpy as np
import matplotlib.pyplot as plt
from testutil import run_gasoline

#Test scripts
import shocktube
import sedovblast
import onestar

if __name__ == '__main__':
	start_time = time.clock()
#Run the shock tube
	testdir='shocktube'
	files = ["data/shocktube.std", "data/shocktube.param"]
	exe = "../gasoline.ce92ce93"
	run_gasoline(testdir, files, 'shocktube.param', exe, args="-sz 4")
	#shocktube.make_plots(testdir)
#Run the Sedov blast
	testdir='sedov'
	files = ["data/sedov.std", "data/sedov.param"]
	exe = "../gasoline.ce92ce93"
	run_gasoline(testdir, files, 'sedov.param', exe, args="-sz 4")
	sedovblast.make_plots(testdir)
#Run the onestar
	testdir='onestar'
	files = ["data/onestar.tbin", "data/onestar.param"]
	exe = "../gasoline.ce92ce93"
	run_gasoline(testdir, files, 'onestar.param', exe, args="-sz 4")
	onestar.make_plots(testdir)
	end_time = time.clock()
	print "Finished all Tests."
	print "Total runtime was %d seconds" % int(end_time-start_time)
