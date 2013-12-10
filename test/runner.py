#!/usr/bin/env python
import os
import sys
import copy
import glob
import time
import select
import shutil
import itertools
import multiprocessing

from collections import deque
from ConfigParser import ConfigParser

import logging
logger = multiprocessing.log_to_stderr()
logger.setLevel(multiprocessing.SUBWARNING)

from lib.tarantool_server import TarantoolServer, LuaTest
from lib.server import Server

threads = 2
class MockArgs(object):
    test = [""]
    suite = []
    suites = []
    is_force = False
    start_and_exit = False
    gdb = False
    valgrind = False
    builddir = ".."
    vardir = "varpal"
    mem = False


class Manager(object):
    def __init__(self, threads):
        self.threads = threads
        self.avail_names = ["Process-" + str(i) for i in xrange(self.threads)]
        self.mpio = multiprocessing.Queue()
        self.running = {}
        self.server = None

    def search_suits(self):
        self.suits = []
        for root, dirs, names in os.walk(os.getcwd()):
            if 'suite.ini' in names:
                self.suits.append(os.path.basename(root))

        self.suits = [TestSuiteParal(suite, self.server) for suite in sorted(self.suits)]
        self.suits = [x for x in self.suits if 'parallel' in x.ini and x.ini['parallel']]

    def run_suites(self):
        while True:
            if not self.avail_names:
                break
            suite = self.suits.pop()
            proc_name = self.avail_names.pop()
            proc = multiprocessing.Process(name=proc_name,
                                           target=TestSuiteParal.run_all,
                                           args=(suite, self.server,
                                                 self.mpio, proc_name))
            proc.start()
            self.running[proc_name] = (suite, proc)

    def run_tests(self):
        while True:
            self.run_suites()
            msg = self.mpio.get()
            print msg
            if msg[0] == 'error' or msg[0] == 'failed':
                exit(1)
            elif msg[0] == 'finished':
                proc_name = msg[1]
                suite, proc = self.running[proc_name]
                self.avail_names.append(proc_name)
                self.suits.append(suite)
                proc.join()
                self.running.pop(proc_name)

    def run_server(self):
        self.server = Server('tarantool')
        self.server.deploy('paral/tarantool.cfg',
                      self.server.find_exe(MockArgs.builddir, silent=False),
                      MockArgs.vardir, False, False,
                      False, False,
                      False, silent=False)

    def stop_server(self):
        self.server.stop(silent=False)
        #self.server.cleanup()

    def __del__(self):
        for _, v in self.running.iteritems():
            v[1].terminate()
        self.stop_server()

class TestSuiteParal(object):
    def __init__(self, suite_path, server):
        self.tests = []
        self.args = MockArgs()
        self.ini = {'server':server}
        self.suite_path = suite_path

        assert(os.path.isdir(self.suite_path))

        config = ConfigParser()
        config.read(os.path.join(self.suite_path, 'suite.ini'))
        self.ini.update(dict(config.items('default')))

        for i in ["config", "init_lua"]:
            self.ini[i] = os.path.join(suite_path, self.ini[i]) if i in self.ini else None
        for i in ["disabled", "valgrind_disabled", "release_disabled"]:
            self.ini[i] = dict.fromkeys(self.ini[i].split()) if i in self.ini else dict()
        for i in ["lua_libs"]:
            self.ini[i] = map(lambda x: os.path.join(suite_path, x),
                    dict.fromkeys(self.ini[i].split()) if i in self.ini else
                    dict())

        self.ini['servers'] = {'default' : server}
        self.ini['connections'] = {'default' : [server.admin, 'default']}
        self.ini['vardir'] = self.args.vardir
        self.ini['builddir'] = self.args.builddir
        for i in self.ini['lua_libs']:
            shutil.copy(i, self.args.vardir)

        self.find_tests()

    def find_tests(self):
        self.tests = [LuaTest(k, self.args, self.ini) for k in sorted(glob.glob(os.path.join(self.suite_path, "*.test.lua")))]

    def run_all(self, server, queue, num):
        print "start", num
        self.queue = queue
        self.server = server
        self.queue.put(('started', num))
        if not self.tests:
            return 0
        try:
            for test in self.tests:
                print "running ", test.name
                a = test.run(self.server)
                #self.queue.put(a)
                if not test.passed():
                    self.queue.put(('failed', num, test.name, a))
                else:
                    self.queue.put(('passed', num, test.name, a))
        except Exception as e:
            self.queue.put(('error', num, e))
            return 1
            #raise
        print "finished"
        self.queue.put(('finished', num))

manager = Manager(2)
manager.run_server()
manager.search_suits()
manager.run_tests()
manager.stop_server()
