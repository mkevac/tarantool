provider lua_cjson {
	probe start();
	probe end(int, char *);
};
provider coro {
	probe init();
	probe start();
	probe end();
};
provider ev {
	probe tick__start(int flags);
	probe tick__stop(int flags);
};
