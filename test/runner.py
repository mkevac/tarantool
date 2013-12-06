#!/usr/bin/env python
import os
import sys
import copy
import glob
import time
import select
import traceback
import multiprocessing
from ConfigParser import ConfigParser

#from lib.test_suits import TestSuite
#from lib.test_runner import SMTH
from lib.tarantool_server import TarantoolServer, LuaTest
from lib.server import Server

threads = 1

class MockArgs(object):
    test = [""]
    suite = []
    suites = []
    is_force = False
    start_and_exit = False
    gdb = False
    valgrind = False
    builddir = ".."
    vardir = "abcdy"
    mem = False

def search_suits(server):
    suites = []
    for root, dirs, names in os.walk(os.getcwd()):
        if 'suite.ini' in names:
            suites.append(os.path.basename(root))

    suites = [TestSuite(suite, server for suite in sorted(suites)]
    suites = [x for x in suites if 'parallel' in x.ini and x.ini['parallel']]
    print suites
    return suites

def run_tests(suits, tnt_serv):
    objs = {}
    print objs
    for suite in suits:
        for i in xrange(threads):
            objs[i] = [multiprocessing.Queue(), copy.deepcopy(suite)]
            objs[i].append(multiprocessing.Process(target=objs[i][1].run_all, args=(tnt_serv, objs[i][0], i)))
            objs[i][2].name = str(i)
            objs[i][2].start()
            print objs[i]
        import traceback
        while objs:
            (ready, [], []) = select.select([q._reader for q in zip(*objs.values())[0]], [], [])
            print ready
            for i in ready:
                msg = i.recv()
                print msg
                if msg[0] == 'error' or msg[0] == 'finished':
                    objs.pop(msg[1])
    return


class TestSuite(object):
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

        self.find_tests()

    def find_tests(self):
        self.tests = [LuaTest(k, self.args, self.ini) for k in sorted(glob.glob(os.path.join(self.suite_path, "*.test.lua")))]

    def run_all(self, server, queue, num):
        self.queue = queue
        self.server = server
        self.queue.put(('started', num))
        if not self.tests:
            return 0
        try:
            for test in self.tests:
                test.run(self.server)
                if not test.passed():
                    self.queue.put(('failed', num, test.name, 'error'))
                else:
                    self.queue.put(('passed', num, test.name))
        except Exception as e:
            self.queue.put(('error', num, e))
            raise
        self.queue.put(('finished', num))


server = Server('tarantool')
server.deploy('paral/tarantool.cfg',
                            server.find_exe(MockArgs.builddir, silent=False),
                            MockArgs.vardir, False, False,
                            False, False,
                            False, silent=False)
suits = search_suits(server)

run_tests(suits, server)


server.stop(silent=False)
server.cleanup()
