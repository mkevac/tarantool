provider tarantool {
	probe start();
	probe end(int, char *);
	probe init();
	probe done();
	probe tick__start(int flags);
	probe tick__stop(int flags);
};
