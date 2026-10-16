#ifndef UPDATER_H_
#define UPDATER_H_

#include <map>
#include <string>
#include <vector>

// The online updater: check a server, fetch a package, prove it arrived intact, install it.
//
// Deliberately free of Aurora, of pge, and of anything that draws. The game and the standalone
// Updater EBOOT are two different builds that must agree on every byte of the protocol, and the only
// way to guarantee that is one implementation compiled into both -- so this file may not depend on a
// renderer, a texture, or a state manager. It uses sceIo, sceHttp (LAN http://), mbedtls
// (GitHub https://), the net modules, zlib's crc32, and the logger, and nothing else.
//
// ---------------------------------------------------------------------------------------------
// The two facts that shape the whole design (see changes.md for the long version):
//
// 1. You cannot replace the file you are running. The PSP maps EBOOT.PBP in order to execute it, so
//    the running game can neither overwrite nor rename it. Downloading can therefore happen live
//    inside the game, but *installing* always needs a second program that is not the one being
//    replaced. That is the whole reason Updater/ exists as its own EBOOT.
//
// 2. The assets are loose files beside the EBOOT, not inside the PBP. `pack-pbp` puts only the SFO,
//    two images and the PRX into the PBP; `Assets/` is ~130 MB of ordinary files next to it. So a
//    package is a *changed-files patch*, never a full distribution, and it carries a manifest saying
//    exactly which files it is allowed to touch.
// ---------------------------------------------------------------------------------------------
//
// The wire format is deliberately line based and pipe separated, not JSON:
//
//     update.txt (what the server offers)         manifest.txt (inside the package)
//     ------------------------------------        ------------------------------------
//     version|3.0.2                               version|3.0.2
//     code|30002                                  code|30002
//     file|MineCraft-3.0.2.zip                    file|EBOOT.PBP|4862715|305419896
//     size|5432109                                file|Assets/.../terrain.png|8123|2596069104
//     crc|305419896                               delete|Assets/old_thing.png
//     full|1
//     patch|MineCraft-3.0.2-from-3.0.1.zip|5234|987654321|3.0.1
//
// The `patch` line is repeatable and is the only record with more than one value. The keys above it
// describe the primary package -- the full install, in a release that offers patches -- and each
// `patch|file|size|crc|base` adds a cheaper alternative for clients on exactly `base`. See PatchOffer.
//
// A pipe, because PSP filenames may contain spaces ("Nostalgia Craft") and a space-separated format
// would need quoting, and because writing a correct JSON parser under -fno-exceptions is a lot of
// code to get subtly wrong for no gain. Both files are produced by one script we control.
//
// crc32 (zlib, already linked for pgeZip) rather than a hash: it catches a truncated download and a
// corrupt zip, which is what can actually go wrong over a LAN. It is not a security boundary, and it
// is not pretending to be one -- the GitHub channel, which is public and unattended, is where a real
// digest belongs.

namespace Update
{
	// The two names the game and the Updater EBOOT have to agree on, so they live next to the
	// protocol rather than being spelled out at each use.
	//
	// PackageName is the staging name a package is always saved as, whatever the server called it.
	// That is what lets the handoff work: the Updater is a separate program with no idea what the
	// server's filename was, so the download has to land somewhere predictable.
	//
	// ExecutableName is the one file a running game cannot replace -- it is the file being executed.
	// A package containing it can only be applied by the Updater, which is why the Updater is the only
	// thing that installs anything any more.
	inline const char* PackageName() { return "update.zip"; }
	inline const char* ExecutableName() { return "EBOOT.PBP"; }

	// Where a card with no update_url.txt looks. GitHub's latest/download/ folder is a static
	// directory of release assets -- the same shape as the LAN server -- and the hop from "latest"
	// onto the current tag is a redirect the HTTP client follows. Updates are published as
	// Releases on the game repo itself.
	inline const char* DefaultReleaseUrl()
	{
		return "https://github.com/TimmyTurner51/PSP-Minecraft/releases/latest/download/";
	}

	// The Updater EBOOT's filename, inside the game folder. Not "EBOOT.PBP", because that name in
	// that folder is the game -- and the folder can only hold one file of that name.
	//
	// It lives beside the game rather than in a folder of its own so the game can address it without
	// knowing anything about where on the card it was installed: the game has a working directory, so
	// this name resolved against it is always correct. The cost is that it is not separately
	// launchable from the XMB, so a game whose EBOOT is broken cannot be repaired from the console
	// alone. That is a deliberate trade and the one thing to revisit first if recovery from the XMB
	// ever matters more than the simpler path.
	inline const char* UpdaterExecutableName() { return "UPDATER.PBP"; }

	// One file inside a package.
	struct FileEntry
	{
		std::string path;       // relative to the game folder, with '/'
		unsigned int size;
		unsigned int crc;
	};

	// What a package says it contains. Parsed from `manifest.txt` inside the zip.
	struct Manifest
	{
		int versionCode;
		std::string version;
		std::vector<FileEntry> files;
		std::vector<std::string> deletes;

		// Set by a `force|1` line in the manifest. Identical in meaning to the same field on Offer: it
		// marks a package that should be applied even when it is not newer than what is installed,
		// which is the normal case while testing a build you have just pushed to the console.
		bool force;

		// Set by a `full|1` line, and it says this package is a complete install rather than a patch.
		//
		// The packaging script writes it when it is run WITHOUT --base, which is the mode it documents
		// for "first release to a card, or when in doubt" -- that build carries every file, so it needs
		// nothing to be applied to and can be extracted into an empty folder.
		//
		// It exists because the launcher has to know which of the two it is holding. A patch must be
		// applied to a COPY of an existing install (so the previous version survives, which is what
		// makes rollback real), and that copy is the most expensive thing the launcher does -- of the
		// order of a minute for the 135 MB an install weighs. Guessing "is this a full package?" from
		// the file list would be a heuristic that is wrong exactly when it matters; the flag is the
		// packaging script stating it, and it is defaulted false so an older package behaves as before.
		bool full;

		// Set by a `base|<version>` line: the version this package was diffed against. Empty for a full
		// package, which by definition needs nothing underneath it.
		//
		// The offer carries the same field, and the manifest carries it as well rather than trusting the
		// offer, because the two are read at different times and the manifest is the one that survives:
		// a package can outlive the update.txt that advertised it, and by then the offer's copy is
		// whatever is on the server now. The manifest's copy is the one that travelled with the bytes.
		std::string base;

	};

	// One alternate package for the same version: a patch diffed against `base`.
	//
	// Why an offer can list more than one package, and why it is worth the extra grammar. A full
	// package of this game is ~95 MB, because it carries every asset, where a patch for one changed
	// line of code is a few megabytes. Before this existed an offer described exactly one package, so a
	// server had to choose: advertise the patch and strand everyone whose installed version did not
	// match its base, or advertise the full package and make everyone download 95 MB to get five files.
	// It could not do both, and the second is what it did.
	//
	// A `patch|` line each lets a release advertise the full package AND a patch per version it can
	// upgrade from. Which one is used is the client's decision, because only the client knows what is
	// on the card; see ChoosePackage below.
	struct PatchOffer
	{
		std::string file;		// relative to the base URL, exactly as Offer::file is
		unsigned int size;
		unsigned int crc;
		std::string base;		// the version this must be applied to; never empty
	};

	// What the server says is available. Parsed from `update.txt`.
	struct Offer
	{
		int versionCode;
		std::string version;
		std::string file;       // the package's filename, relative to the base URL
		unsigned int size;
		unsigned int crc;

		// Set by a `force|1` line, which asks the game to offer this package even when its version is
		// not newer than the running one.
		//
		// This exists for the development loop and for little else. While working on a build, the only
		// thing you have to push is the one you just made, so the server advertises exactly the version
		// the console is already running -- and the normal comparison then answers "up to date" and
		// refuses to install the very thing you are trying to test. Setting it changes that one
		// comparison and nothing else: the size and crc checks, the manifest, the extractor and the
		// EBOOT.PBP refusal all still apply, so a forced install is not a less careful one.
		//
		// Default false, so an offer that does not mention it behaves exactly as before and a release
		// package can never be forced by accident.
		bool force;

		// Set by `full|1` and `base|<version>`, mirroring the manifest so the launcher can tell what it
		// is being offered WITHOUT downloading it first.
		//
		// That is the point of duplicating them here. The manifest lives inside the zip, so before these
		// existed the only way to learn whether an offer was a 5 MB patch or a 95 MB full install was to
		// download all of it -- which is the wrong order when the answer changes whether the download
		// should happen at all. With them in the offer, a patch can be refused outright on a card that
		// has nothing to patch it onto, and the confirmation can say which kind it is.
		//
		// Absent from the offer (an older server) leaves full false, i.e. "assume a patch" -- the safe
		// direction, because a patch that turns out to be full still installs; a full package assumed to
		// be a patch would be refused.
		bool full;
		std::string base;

		// Set by `target|launcher`: this offer is the launcher updating ITSELF, not a game install.
		//
		// The launcher is the file the XMB runs, so it is the one package it cannot apply the ordinary
		// way -- a running program cannot replace the file it is executing (see the note at the top of
		// this header). Everything else in the protocol is unchanged; this flag only tells the launcher
		// which of its two installers to walk into. It is a separate key rather than a naming
		// convention on `version` because the two destinations are different folders and the difference
		// has to be stated, not inferred -- inferring it from "the manifest mentions EBOOT.PBP" would
		// make any game package that happens to carry the root executable look like a launcher update.
		//
		// Default false, so every server written before this existed behaves exactly as before.
		bool launcher;

		// Extra packages for this same version, each tied to the version it must be applied to. Empty
		// from a server that offers a single package, which is every server written before this
		// existed -- so an offer with no `patch` lines behaves exactly as it always did.
		std::vector<PatchOffer> patches;
	};

	// The package actually chosen to fetch, once the offer's alternatives have been weighed against
	// what is installed. Field for field what the download and the confirmation need.
	//
	// This exists so the caller stops reading the offer the moment it is picked. Reading `offer.size`
	// after choosing would print the full package's size beside the patch's filename, which is the
	// kind of mistake that shows up as a progress bar lying about how long the wait is.
	struct Package
	{
		std::string file;
		unsigned int size;
		unsigned int crc;
		bool full;
		std::string base;		// empty for a full package
	};

	// The package to fetch for an install of `installedVersion`, given what the server offers.
	//
	// Prefers the patch whose `base` is exactly that version, because it is the small one, and falls
	// back to the offer's primary package otherwise -- which is what makes a version the client does not
	// have still installable. An empty `installedVersion` (nothing installed, or nothing to patch)
	// always takes the primary.
	//
	// `allowPatch` is false for a repair, and that is not a tuning knob. A patch carries only the files
	// that DIFFER between its base and the new version, so a file that was deleted from the card but
	// did not change between those two versions is absent from it: the repair would write the files it
	// does carry, verify them, report success, and leave that file missing. The primary package carries
	// every file, and the installer's per-file size and crc check rewrites exactly the damaged ones --
	// so repair is both safer and cheaper against the full package, not merely simpler.
	//
	// A patch is never `full`, whatever the offer's primary package is. Choosing one means the caller
	// will take the copy-then-patch path, which is what a patch requires.
	Package ChoosePackage(const Offer& offer, const std::string& installedVersion, bool allowPatch);

	// Draws one frame and returns false if the player asked to stop. A blocking network call has to
	// be able to both paint and be cancelled, and this module cannot know how to do either -- the
	// caller owns the renderer and the input. Returning false must abort the operation.
	typedef bool (*FrameFn)(void* ctx);
	// Called during a transfer with the byte counts so far. `total` is 0 when the server did not send
	// a Content-Length. Return false to cancel; the partial file is then removed, because a half
	// written zip that survives would be mistaken for a complete one on the next attempt.
	typedef bool (*ProgressFn)(void* ctx, unsigned int done, unsigned int total);

	// ---------------------------------------------------------------------------------------
	// Network lifetime. Initing the net modules and bringing up a connection are separate jobs:
	// NetInit() only loads the modules, and the player still has to choose an access point.
	// ---------------------------------------------------------------------------------------

	// Loads the net modules and inits sceNet/sceNetInet/sceNetApctl/sceNetResolver. Safe to call
	// twice; NetShutdown() undoes it. Returns false if any step failed (networking unavailable).
	bool NetInit();
	void NetShutdown();

	// The last updater: line written by the HTTP client. Empty until a fetch has failed (or
	// succeeded and logged something). Shown on the "could not reach" dialog so a card whose
	// Log.txt Windows cannot open still names the error.
	const char* LastHttpError();

	// Runs the PSP's own Network Connection utility, which is the only supported way to pick an
	// access point from inside a game -- homebrew cannot read the saved connection list, so showing
	// the system dialog is not a shortcut, it is the interface.
	//
	// Two frame callbacks rather than one, because the dialog is pumped between them: `clear` empties
	// the buffer the dialog draws into, then sceUtilityNetconfUpdate has the dialog draw, then
	// `present` waits for vblank and swaps. Doing both halves on the same side of the pump -- which is
	// what a single callback forces -- clears the frame that was just drawn and shows the empty buffer,
	// and that is what tore the connection dialog and the keyboard. `clear` needs no knowledge of the
	// dialog at all, and neither callback may paint the game's own screen: the dialog's threads write
	// that same framebuffer. Either returning false aborts. Returns true only if a connection is
	// actually up afterwards, which is checked rather than assumed.
	bool NetConnectDialog(FrameFn clear, FrameFn present, void* ctx);

	// True when there is a live connection with an address. Does not imply the server is reachable.
	bool NetIsConnected();

	// ---------------------------------------------------------------------------------------
	// Connecting without the dialog.
	//
	// The note above calls the system dialog "the interface" because it was believed homebrew could
	// not read the console's saved connections. That is not true: sceUtilityCheckNetParam and
	// sceUtilityGetNetParam read the saved list from user mode, and sceNetApctlConnect joins one by
	// index with nothing on screen. What the dialog remains is the fallback -- the way to add or pick
	// a connection the console has never used, and the only path that can ask the player anything.
	//
	// The value of the silent path is that it can run before the player has asked for anything, which
	// is what lets the menu find out about a release instead of waiting to be told to look. None of it
	// can tell whether a saved connection exists but is out of range, so a failure here is ordinary
	// and must leave the caller exactly where it started.
	// ---------------------------------------------------------------------------------------

	// Is there a saved connection at `index` (1..10)? Uses sceUtilityCheckNetParam.
	//
	// Do NOT assume the list is numbered 1..count with no gaps, which is what the first version of this
	// did and why the launcher reported "no saved connections" on a console that plainly had one: the
	// list can start above 1, and deleting an entry leaves the rest where they were. Everything that
	// walks the list therefore walks 1..10 and asks per index.
	bool NetConnectionValid(int index);

	// How many saved connections exist, for logging. Not an upper bound on the indices -- see above.
	int NetConnectionCount();

	// The name the console shows for saved connection `index`. False if it cannot be read.
	bool NetConnectionName(int index, std::string* name);

	// Joins saved connection `connIndex` with no dialog. `pump` is called once per poll so the caller
	// can keep painting and can cancel by returning false. True only when an address is actually up,
	// which is checked rather than assumed -- the state machine can reach GOT_IP and still have no
	// usable address.
	bool NetConnectSilent(int connIndex, FrameFn pump, void* ctx, unsigned int timeoutMs);

	// Leaves the current connection, so the next candidate starts from nothing rather than from whatever
	// the last one half-established. Called after a connection turns out not to reach anything.
	void NetDisconnect();

	// The address the console holds right now, "" when there is none. The boot connect uses it to tell
	// a network that can reach the update server apart from one that merely associated: joining a
	// network says nothing about being on the same LAN as the server, and only the address distinguishes
	// the two.
	bool NetCurrentAddress(std::string* address);

	// Logs the console's saved connections: each id, the raw result of sceUtilityCheckNetParam for it,
	// and the name if one can be read.
	//
	// This exists because "the launcher found no saved connections" is a claim that can be wrong in two
	// different ways -- the console really has none, or the enumeration is not answering as expected on
	// this firmware -- and the two want opposite fixes. The log line is what tells them apart, since
	// neither is reproducible anywhere but on the console.
	void NetLogConnections();

	// ---------------------------------------------------------------------------------------
	// HTTP. Both calls block, and both call `progress` between chunks.
	// ---------------------------------------------------------------------------------------

	// Fetches a small text resource (update.txt) into memory. Body is capped at 16 KB: the file is
	// six lines, and a cap turns "the server sent an error page / the wrong file" into a failure
	// instead of an allocation the PSP cannot satisfy.
	bool HttpGetToString(const std::string& url, std::string* out, ProgressFn progress, void* ctx);

	// Fetches a file to disk. The destination is opened with PSP_O_TRUNC, which is the fix for a real
	// bug in pgeNetGetFile: without it a re-download leaves the previous file's tail in place and the
	// result is a corrupt zip that is exactly as large as the longer of the two.
	bool HttpGetToFile(const std::string& url, const std::string& destPath, ProgressFn progress, void* ctx);

	// A partial GET into memory: `start`..`end` inclusive, or `end` -1 for "from start to the end of the
	// file". Only HTTP 206 is accepted -- a 200 means the server ignored the Range header and is sending
	// the WHOLE resource, which is refused rather than consumed, because accepting it would work, be
	// slow, and leave no trace of the fact that the design had degraded into the thing it replaced.
	bool HttpGetRangeToString(const std::string& url, long start, long end, std::string* out,
							  unsigned int maxBytes, ProgressFn progress, void* ctx);

	// ---------------------------------------------------------------------------------------
	// The protocol, as pure functions. No network, no sceIo, no PSP. These are the parts worth
	// being able to test without a device.
	// ---------------------------------------------------------------------------------------

	bool ParseOffer(const std::string& text, Offer* out);
	bool ParseManifest(const std::string& text, Manifest* out);

	// ---------------------------------------------------------------------------------------
	// A published release, and the index of a remote zip.
	//
	// The division of labour these exist for: the server is a static file host publishing zips and a
	// list of them, like a releases page, and is given nothing to obey. Every decision -- which release,
	// which file, whether anything is needed at all -- is made on the console, and it begins by asking
	// "what is inside that zip?" without downloading it.
	// ---------------------------------------------------------------------------------------

	// One file inside a remote zip, as the archive's central directory describes it.
	//
	//   crc                 CRC32 of the uncompressed bytes; what a local file is compared against
	//   compressedSize      bytes to fetch, if this entry ever has to be fetched
	//   uncompressedSize    bytes it inflates to
	//   localHeaderOffset   where the entry's data begins, relative to the start of the archive
	//   method              0 stored, 8 deflate
	struct RemoteEntry
	{
		std::string name;
		unsigned int crc;
		unsigned int compressedSize;
		unsigned int uncompressedSize;
		unsigned int localHeaderOffset;
		unsigned int method;
	};

	// One release as releases.txt publishes it: a version and its assets, with the size of each. The
	// sizes are here because a range request has to know the archive's length to find its index, and
	// carrying them in the list removes any need for a HEAD round trip or a size probe.
	struct ReleaseInfo
	{
		std::string version;
		int versionCode;

		std::string gameZip;
		unsigned int gameZipSize;

		std::string launcherZip;
		unsigned int launcherZipSize;

		std::string shotsZip;
		unsigned int shotsZipSize;
	};

	bool ParseReleases(const std::string& text, std::vector<ReleaseInfo>* out, std::string* error);

	// Reads a remote zip's central directory, fetching only the end of the archive. This is the call
	// that makes a release's contents knowable for a hundred-odd KB instead of a hundred-odd MB.
	bool FetchZipIndex(const std::string& url, unsigned int zipSize,
					   std::vector<RemoteEntry>* out, std::string* error);

	// Fetches and logs a remote zip's index and writes nothing at all. The measurement it prints --
	// bytes on the wire to learn a whole archive's contents -- is the point of the arrangement.
	bool InspectRemoteZip(const std::string& url, unsigned int zipSize);

	// ---------------------------------------------------------------------------------------
	// Repairing an install by fetching only what is wrong with it.
	//
	// This is what the index above is for. Downloading a 94.5 MB archive to replace three files is the
	// behaviour this replaces, and the measurements that justify it are all in one place: the record
	// names the files, a tree scan says which are missing or the wrong length, the index says where each
	// of those lives in the archive and what it compresses to, and only those ranges are fetched.
	// ---------------------------------------------------------------------------------------

	// True for the paths the game rewrites while it runs: its saved worlds, its input config, its
	// terrain-probe tiles and its own logs.
	//
	// These are excluded from the manifest AND from the integrity check, and both halves are needed. A
	// file the game is entitled to change can never be evidence that an install is broken, and treating
	// one as evidence is not theoretical: 39 of the 42 files the check flagged on a healthy install were
	// exactly these, so the install could never read as complete and the repair prompt fired on every
	// boot. Excluding them from the check is what lets "complete" mean complete.
	//
	// Folded case-insensitively, because the card is, and front-anchored so that a file merely containing
	// a folder's name is not swept up with it.
	bool IsRuntimePath(const std::string& path);

	// The archive entries an install does not already hold, judged against the ARCHIVE rather than
	// against the install's own record.
	//
	// WHY NOT THE RECORD, which is the obvious source and is what this did first. The install's record
	// describes the release the install was made from -- the OLD one -- so diffing against it repairs the
	// card back towards the old release, then writes the record forward to the new release's manifest,
	// and leaves the check to flag the very files it just restored. Converging then takes one repair
	// round per release missed, and each round is only correct by accident. Diffing against the archive
	// asks the question that actually matters: does the card already hold what THIS release contains?
	//
	// Three ways an entry is wanted: the card does not have it, it has it at a different length, or the
	// record says its content is not the archive's -- a different crc at the same length. Size alone
	// cannot see the third, and the record is the only cheap source of content: reading every file to crc
	// it is the 130 MB walk the record exists to avoid.
	//
	// TreeScan's definition sits lower in this header; it is forward-declared here because this block
	// only ever holds one by reference.
	struct TreeScan;

	void CollectArchiveDiff(const TreeScan& scan, const Manifest& record,
							const std::vector<RemoteEntry>& index,
							std::vector<std::string>* out);

	// A repair worked out before anything is downloaded.
	struct RepairPlan
	{
		std::vector<RemoteEntry> entries;	// the archive entries to fetch, in record order
		unsigned long long bytes;			// their compressed size: what the repair will actually cost
		bool usable;						// false when the archive cannot answer for every bad file

		// The archive's own manifest.txt, and whether the archive carries one.
		//
		// THE RECORD HAS TO BE UPDATED AS PART OF A REPAIR, and this is the entry that does it. The
		// record is what the integrity check measures the card against, so a repair that fixed the files
		// but left the record describing the PREVIOUS release would re-flag every file it had just
		// written -- and offer the same repair again, forever. It is not a concern for a repair that
		// reinstalls the version already recorded (that is self-consistent by accident), which is
		// exactly why it would have gone unnoticed until the first genuine release.
		RemoteEntry record;
		bool hasRecord;

		RepairPlan() : bytes(0), usable(false), hasRecord(false) {}
	};

	// Matches the bad files against the archive's index. Nothing is downloaded here beyond the index
	// itself, which the caller has already paid for, so a repair can be priced -- and refused -- before
	// it is committed to. Every bad file has to be in the index; a single one that is not makes the plan
	// unusable rather than partial, because a repair that silently skips a file leaves an install that
	// still does not work and no record of why.
	bool PlanRangeRepair(const std::vector<std::string>& badPaths,
						 const std::vector<RemoteEntry>& index, RepairPlan* out, std::string* error);

	// Fetches those entries by range, inflates each with the already-linked zlib, verifies it against the
	// index's crc, and moves it into place. Only the compressed bytes of an entry are ever held in RAM;
	// the uncompressed side is streamed to the card, because the largest entry in the archive is 13 MB
	// and this program's heap is 8.
	bool ApplyRangeRepair(const std::string& url, unsigned int zipSize,
						  const std::string& installFolder, const RepairPlan& plan,
						  ProgressFn progress, void* ctx, std::string* error);

	// ---------------------------------------------------------------------------------------
	// Files.
	// ---------------------------------------------------------------------------------------

	// The folder the running game lives in, read from getcwd() rather than assumed. The game loads
	// "Assets/..." relatively, so the working directory *is* the game folder; this exists so the
	// answer is taken from the process instead of being hardcoded a second time.
	bool GameDirectory(std::string* out);

	// crc32 of a file plus its size, in one pass and one buffer.
	bool CrcOfFile(const std::string& path, unsigned int* outCrc, unsigned int* outSize);

	// Reads a package's own manifest without extracting anything. Callers use it to find out what a
	// package is *going* to do before letting it do it -- in particular whether it contains
	// EBOOT.PBP, which is the file a running game cannot replace (see the note at the top of this
	// header). A package that changes code is applied by the Updater EBOOT; one that only changes
	// assets can be applied in place.
	bool ReadManifestFromZip(const std::string& zipPath, Manifest* out, std::string* error);

	// Applies an already-downloaded package in place. Reads `manifest.txt` from inside the zip,
	// extracts every entry, honours the delete list, then verifies what landed.
	//
	// Returns false only when the package itself could not be used (unreadable, no manifest, a
	// manifest that does not parse) or when verification finds a file wrong. Per-file extraction
	// failures surface through that verification, which is deliberate: it means the caller is told
	// *which* file is missing, and the one it will be is the running EBOOT.PBP.
	//
	// Note that this can leave a half-applied tree if it fails part way, which is why callers should
	// ask ReadManifestFromZip first and refuse anything that touches EBOOT.PBP while the game is
	// running.
	//
	// `progress` is called with the running byte count as each file is written, so a caller can show a
	// bar that moves. Its RETURN VALUE IS IGNORED, and that is deliberate rather than an oversight:
	// the download path uses the return to let the player cancel, and there is no safe point to cancel
	// in here. Stopping between two files leaves a package half applied, and stopping in the middle of
	// one leaves a truncated file that verification would then fail on -- so the only place a cancel
	// could be honoured is before this call, which is where the caller already offers it.
	bool InstallPackage(const std::string& zipPath, Manifest* outManifest, std::string* error,
						ProgressFn progress = 0, void* ctx = 0);

	unsigned int CrcOfBuffer(const void* data, unsigned int len);

	// Everything a manifest promises, actually on disk. Reports the first file that is wrong, by
	// name, because "the update failed" is useless and "Assets/.../terrain.png is 8123 bytes, not
	// 4096" is not.
	bool VerifyInstalled(const Manifest& m, std::string* error);

	// ---------------------------------------------------------------------------------------
	// What an install contains, and whether it still contains it.
	//
	// A package's manifest is a DELTA. For a patch it lists only the files that changed, so it cannot
	// answer "is this install complete?" -- and that is the question the launcher has to answer before
	// it can offer to repair an install instead of starting it.
	//
	// So an install keeps its own manifest at the top of its folder: the accumulating union, seeded by
	// whichever package created it and folded forward by each patch applied over it. It is the only
	// description of an install that outlives the package that made it, and it is what lets a missing
	// file be found without downloading anything.
	// ---------------------------------------------------------------------------------------

	// Where an install keeps that record, relative to the install folder.
	inline const char* InstallManifestName() { return "manifest.txt"; }

	// Folds a just-applied package's manifest into the record in the CURRENT DIRECTORY, which is the
	// install folder for the duration of an install. InstallPackage calls this once verification has
	// passed, so the record and the tree cannot be left disagreeing.
	//
	// The merge is not a formality. A patch names a handful of files, and replacing the record with it
	// would shrink the install's description to those files -- after which the check below would stop
	// noticing everything else, which is the silent failure this whole mechanism exists to prevent.
	// Entries the package wrote are updated, entries it deleted are dropped, the rest are kept.
	//
	// Returns false, with `error` set, only if the record could not be written. That is a warning and
	// not a failed install: every file verified before this is reached.
	bool RecordInstalledManifest(const Manifest& applied, std::string* error);

	// Reads an install's record out of its folder. Returns false when there is none, which is the state
	// of every install made before the record existed and of one copied onto the card by hand. Callers
	// must read that as "unknown", never as "broken".
	bool ReadInstalledManifest(const std::string& installFolder, Manifest* out);

	// What comparing an install's record against the card found.
	struct Integrity
	{
		unsigned int checked;		// entries looked at
		unsigned int missing;		// named by the record, not on the card
		unsigned int wrongSize;		// on the card at a different length
		std::string firstProblem;	// the first offender, in words, for the log

		Integrity() : checked(0), missing(0), wrongSize(0) {}

		bool Ok() const { return missing == 0 && wrongSize == 0; }
	};

	// ---------------------------------------------------------------------------------------
	// A whole-tree comparison: the question CheckInstalledSizes used to answer, asked in a way a memory
	// stick can actually keep up with.
	//
	// WHY THIS IS NOT A LOOP OF sceIoGetstat, which is what it replaced and what it looks like it ought
	// to be. The record names ~1500 files, and asking the card about each one BY NAME resolves a path
	// from the root every time -- six directory levels for the deeper entries -- at a few milliseconds
	// each on a real card. Sliced at 4 ms a frame that is around 1500 frames, so half a minute of the
	// menu saying "Play" before it admits the install is broken, which the player cannot tell apart from
	// a launcher that is never going to notice. Measured on hardware, that is what happened.
	//
	// Reading the DIRECTORIES instead costs one open per directory -- 227 of them for the install this
	// was written against -- and one read per entry, and each entry already carries the file's length,
	// so the whole tree is described without a single path lookup. It is also naturally chunky: the
	// largest directory in that tree holds 89 entries, so a step completes a directory at a time and no
	// single frame has a stall to hide behind.
	//
	// Existence and length only, where VerifyInstalled reads every byte to check its crc as well. That
	// difference is the reason both exist: this runs while a menu is being drawn and cannot read 130 MB
	// to decide what a button should say, and "the file is gone" and "the file is the wrong length" are
	// what a repair is actually for. crc still decides whether an install SUCCEEDED, and still decides
	// whether a file needs rewriting during one.
	// ---------------------------------------------------------------------------------------
	struct TreeScan
	{
		std::string root;							// absolute install folder, no trailing slash
		std::vector<std::string> dirs;				// install-relative, still to read; "" is the root
		std::map<std::string, unsigned int> sizes;	// install-relative path -> its length on the card
		unsigned int dirsRead;						// directories finished, for the log
		unsigned int filesSeen;						// entries that were files, for the log

		TreeScan() : dirsRead(0), filesSeen(0) {}
	};

	// Queues the install's root. Nothing is read here, so starting a scan and never finishing it costs
	// nothing, and the caller is free to decide between frames whether it still wants the answer.
	//
	// `installFolder` is the same prefix CheckInstalledSizes took, for the same reason: the record's
	// paths are relative to the install, and the launcher is not standing in it. The scan is told the
	// prefix once, at the start, rather than per path -- which is the other half of what makes this
	// cheap.
	void BeginTreeScan(const std::string& installFolder, TreeScan* out);

	// Reads directories until `budgetUs` is spent or the queue empties. Returns true when the tree is
	// complete, at which point `sizes` describes every file under the install root.
	bool StepTreeScan(TreeScan* scan, unsigned int budgetUs);

	// Compares the record against a completed scan. `checked` counts RECORD entries rather than files
	// walked: an install may hold files the record does not mention, and those are not a fault -- a
	// saved world is exactly that -- so they are passed over rather than counted against it.
	void CompareTreeScan(const TreeScan& scan, const Manifest& m, Integrity* out);

	// ---------------------------------------------------------------------------------------
	// The handoff: getting from the game into the Updater EBOOT, and back.
	//
	// This is the one part of the protocol that is NOT symmetric, and the asymmetry drives the whole
	// design. The game runs with a working directory and can therefore resolve a relative name
	// against its own folder. The Updater does not: loadexec starts it with no working directory at
	// all, which was measured on hardware -- the probe's payload reported `cwd : "(failed)"`. That
	// one fact has three consequences, and each of them is a bug if it is forgotten:
	//
	//   1. The Updater cannot resolve "UPDATER.PBP", so the game must hand it an absolute game
	//      folder. It cannot find its own folder either, so the handoff file cannot be relative.
	//   2. Aurora::Utils::Logger opens its log by the relative name "Log.txt", so the Updater logs
	//      nothing until it has a working directory.
	//   3. pgeZipExtractCounted writes each archive entry to the path stored in the archive, which is
	//      relative to the game folder. With no working directory there is nowhere to write them.
	//
	// Consequences 2 and 3 are both fixed by one call, sceIoChdir(gameDir), which is why that is the
	// Updater's first act after reading this file. Everything downstream then works unchanged, and
	// deliberately so: the extractor and the manifest's relative paths are shared code that must
	// behave identically in both builds.
	//
	// The file itself is at a FIXED path, because a relative one cannot be found by the program that
	// needs it and a path beside the EBOOT cannot be constructed without finding the EBOOT first.
	// ---------------------------------------------------------------------------------------

	// What the game leaves behind for the Updater. Written by the game when a package needs code
	// installed -- the case the running game cannot do itself.
	struct Pending
	{
		std::string gameDir;		// absolute: the folder holding EBOOT.PBP and Assets/
		std::string package;		// absolute: the staged update.zip
		std::string version;		// for the log line the Updater shows
		int versionCode;			// for comparing against what ends up installed
		std::string state;			// "apply" today; "verify" is reserved for rollback
	};

	// The handoff file's name, without a device. Exposed so the game and the Updater cannot disagree
	// about it -- and so a test script can look for the same name.
	inline const char* PendingFileName() { return "mcupdater-pending.txt"; }

	// Where the Updater leaves its own log, and why it is not "Log.txt".
	//
	// The game is built with MC_DEBUG_MODE, and its boot calls Logger::ResetLog() (DebugHeap.cpp). So
	// the instant the Updater succeeds in starting the game -- the case that matters most -- the game
	// truncates Log.txt and every line describing what the handoff did is gone. The Updater copies its
	// log here first (see PreserveLog() in UpdaterApp/main.cpp), which is the only way that record
	// outlives the reboot it is a record of.
	//
	// It is a second file rather than a change to Logger because Logger is shared with the game and its
	// path is the game's whole log; this is the Updater's own record and it needs to be able to outlive
	// it.
	inline const char* UpdaterLogName() { return "UpdaterLog.txt"; }

	// pgeZip's diagnostics, routed into this module's log.
	//
	// pgeZip.c prints to stdout on every one of its error paths, and a diagnostic that goes nowhere is
	// worse than no diagnostic: the failure then reads as a silent crash, which is exactly the shape
	// this project has had to rule out more than once. The standalone Updater EBOOT has no console to
	// print to, so its build defines PGE_ZIP_LOG to this function (see UpdaterApp/Makefile) and every
	// one of those messages, naming the file and the step, lands in Log.txt beside everything else.
	//
	// Declared with C linkage because the caller is pgeZip.c. It has a plain printf signature and
	// appends nothing, so a message it is handed reads the same as it would have on stdout.
	extern "C" int UpdateZipLog(const char* format, ...);

	// Writes the handoff. Uses the device prefix of `p.gameDir` so a game on ef0: writes where its
	// own Updater will look. Returns false with `error` set if the file could not be written, which
	// the caller must treat as "the handoff did not happen" rather than launching anyway.
	bool WritePending(const Pending& p, std::string* error);

	// Reads the handoff. Tries every device this function knows about, because the Updater cannot
	// derive the device from anything it has. `pathUsed`, when not NULL, receives the path that
	// worked, for the log.
	bool ReadPending(Pending* out, std::string* error, std::string* pathUsed);

	// Removes it, on every device. Called once the install is done, and silently successful when
	// there was nothing to remove.
	void ClearPending();

	// ---------------------------------------------------------------------------------------
	// The launcher's own swap handoff: the one case the launcher cannot do for itself.
	//
	// The launcher installs a package's files itself -- it has a working directory and can write any
	// path in its folder -- with a single exception: its own EBOOT.PBP, which is the file the XMB is
	// executing. That one file has to be moved by a second program, and that program (LauncherUpdater,
	// in Updater/EBOOT.PBP under the launcher root) is deliberately the smallest binary in the project:
	// no renderer, no zlib, no net, no Aurora, nothing that can fail before the swap happens. It also
	// must NOT be a C++ program that links this file, because everything it would gain is a way to not
	// start.
	//
	// Consequently the file it reads is NOT the Pending handoff above and the format is NOT this
	// module's: the two programs share no code by design, and the format is small enough to say in one
	// line. It is written here by the launcher and parsed by the helper, and the two agree on:
	//
	//     <absolute staged EBOOT path>|<absolute target EBOOT path>\n
	//
	// One record, one separator, both paths absolute because the helper has no working directory --
	// the same reason every other cross-program path in this file is absolute. A staged path with a
	// '|' in it would break the split, which cannot happen: the launcher builds both paths itself.
	// ---------------------------------------------------------------------------------------

	// The swap handoff's name, without a device. Fixed, and looked for on every device, for exactly
	// the reasons PendingFileName is: the helper cannot derive a device from anything it has.
	inline const char* LauncherSwapFileName() { return "mclauncher-swap.txt"; }

	// Writes the swap handoff, on the device of `targetExe`. Uses the target's device rather than the
	// staged file's because the target is the launcher and the helper is running beside it, so the two
	// are always on the same device -- and the target is the path the helper is also given, so one
	// device is enough to find both. Returns false, with `error` set, if the handoff could not be
	// written, which the caller must treat as "do not launch the helper" rather than launching anyway.
	bool WriteLauncherSwap(const std::string& stagedExe, const std::string& targetExe, std::string* error);

	// Removes it, on every device. Successful when there was nothing to remove.
	void ClearLauncherSwap();

	// Extracts ONE named entry from a package to `destPath`, without touching any other file, and
	// verifies its size and crc against the manifest entry it names. This is how the launcher stages
	// the new EBOOT.PBP: InstallPackage would write it to the wrong place (its manifest paths are
	// relative to the folder being installed, and EBOOT.PBP must land at a staging name, not over the
	// running file). Returns false with `error` set.
	bool ExtractOneFromZip(const std::string& zipPath, const std::string& entryName,
						   const std::string& destPath, std::string* error);

	// Starts another EBOOT and does not come back.
	//
	// `absolutePath` is required to be absolute, and not as a courtesy: see the note above. On
	// success this does not return -- loadexec resets the console -- so a return ALWAYS means the
	// launch failed, with `error` set. Callers rely on that: the game treats "we came back" as its
	// cue to show the player the manual instructions.
	//
	// The parameters are the ones measured on hardware by tools/loadexec-probe: apitype MS2, key
	// "game" (a NULL key also worked, so the value is not load-bearing, only documented), and unk5
	// at its documented default. The real requirement is the absolute path.
	bool LaunchExecutable(const std::string& absolutePath, std::string* error);
}

#endif
