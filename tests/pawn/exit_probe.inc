#if !defined MMDB_TEST_PINNED_SHUTDOWN
new bool:gMMDBTestShuttingDown;
#endif

stock MMDBTestLogExit(const script[]) {
#if !defined MMDB_TEST_PINNED_SHUTDOWN
	// Local runtimes may stop serving MMDB natives before Pawn exits.
	if (gMMDBTestShuttingDown) return;
#endif

	new bool:loaded, version = -1;
	new MMDBStatus:presenceStatus = mmdb_is_loaded("shared", loaded);
	new MMDBStatus:versionStatus = mmdb_get_ip_version("shared", version);
	printf("MMDB_EXIT %s %d %d %d %d", script, _:presenceStatus, _:loaded, _:versionStatus, version);
}
