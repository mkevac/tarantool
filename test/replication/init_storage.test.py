import os
import glob
from lib.tarantool_server import TarantoolServer

# master server
master = server

master.admin("space = box.schema.create_space('test', {id =  42})")
master.admin("space:create_index('primary', { type = 'hash'})")

master.admin('for k = 1, 9 do space:insert{k, k*k} end')

for k in glob.glob(os.path.join(master.vardir, '*.xlog')):
    os.unlink(k)

print '-------------------------------------------------------------'
print 'replica test 1 (no such space)'
print '-------------------------------------------------------------'

replica = TarantoolServer()
replica.deploy("replication/cfg/replica.cfg",
               replica.find_exe(self.args.builddir),
               os.path.join(self.args.vardir, "replica"),
               need_init=False)

replica.admin('box.space.test')

replica.stop()
replica.cleanup(True)

master.admin('box.snapshot()')
master.restart()
master.admin('for k = 10, 19 do box.space[42]:insert{k, k*k*k} end')
lsn = master.get_param('lsn')
print '-------------------------------------------------------------'
print 'replica test 2 (must be ok)'
print '-------------------------------------------------------------'

replica = TarantoolServer()
replica.deploy("replication/cfg/replica.cfg",
               replica.find_exe(self.args.builddir),
               os.path.join(self.args.vardir, "replica"),
               need_init=False)

replica.admin('space = box.space.test');
replica.wait_lsn(lsn)
for i in range(1, 20):
    replica.admin('space:select{%d}' % i)

replica.stop()
replica.cleanup(True)

server.stop()
server.deploy(self.suite_ini["config"])

