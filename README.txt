  This version is a bit of a departure, hence "gaiden". Configuration
is entirely file-based, in `0xbitcoin.json', which _is_ read by a
proper JSON interpreter, so formatting is
important. https://jsonlint.com/ is quite useful for checking, if you
have any difficulty.

	You will have to supply your own Ethereum address (mine is there as
a placeholder - please don't actually mine to it!) and select a
server.
	CUDA devices are numbered starting at 0, which is the "most capable"
card according to Nvidia's criteria - not always accurate, but
generally at least close. Specific devices can be disabled if desired,
and per-card intensity is, hopefully obviously, supported easily. Have
no worries about specifying cards that don't exist: such settings are
simply ignored.

	Once your configuration is complete, simply double-click on the
miner and enjoy. No more faffing about with arcane batch file syntax,
piping commands and so on. Pool mining only, I'm afraid.

	There is, finally, a ~2.5% dev fee. The implementation is simple and
uses a foolproof (though I tested it anyway!) counter: it merely
diverts every 41st share to my address. Dev shares are not indicated
in your solution counter, but instead trigger a log message containing
a counter so that you can verify the ratio and timing.

  Happy mining!
