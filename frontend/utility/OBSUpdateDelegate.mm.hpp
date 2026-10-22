#include "frontend/build/frontend-sparkle-delegate-mm.pch.hpp"

@implementation OBSUpdateDelegate {
}

@synthesize branch;

/* Stable allows only Sparkle's default channel, which certified appcasts use.
 * Nightly (#1759) also allows the items tagged
 * <sparkle:channel>nightly</sparkle:channel> in the continuous release's feed. */
- (nonnull NSSet<NSString *> *)allowedChannelsForUpdater:(nonnull SPUUpdater *)updater
{
	if (!ObsRustUpdateBranchIsKnown(branch.UTF8String)) {
		return [NSSet set];
	}
	return [NSSet setWithObject:branch];
}

- (void)observeCanCheckForUpdatesWithAction:(nonnull QAction *)action;
{
	[_updaterController.updater addObserver:self
				     forKeyPath:NSStringFromSelector(@selector(canCheckForUpdates))
					options:(NSKeyValueObservingOptionInitial | NSKeyValueObservingOptionNew)
					context:(void *)action];
}

- (void)observeValueForKeyPath:(NSString *)keyPath
		      ofObject:(id)object
			change:(NSDictionary<NSKeyValueChangeKey, id> *)change
		       context:(void *)context
{
	if ([keyPath isEqualToString:NSStringFromSelector(@selector(canCheckForUpdates))]) {
		QAction *menuAction = (QAction *)context;
		menuAction->setEnabled(_updaterController.updater.canCheckForUpdates);
	} else {
		[super observeValueForKeyPath:keyPath ofObject:object change:change context:context];
	}
}

/* nil keeps Info.plist's SUFeedURL (the certified feed). The nightly branch
 * reads the same appcast from the continuous release (ObsRustFeedUrl). */
- (NSString *)feedURLStringForUpdater:(SPUUpdater *)updater
{
	if (![branch isEqualToString:[NSString stringWithUTF8String:OBS_RUST_NIGHTLY_BRANCH]]) {
		return self.feedUrl;
	}
	NSString *stable = self.feedUrl;
	if (stable == nil) {
		stable = [[NSBundle mainBundle] objectForInfoDictionaryKey:@"SUFeedURL"];
	}
	if (stable == nil) {
		return nil;
	}
	std::string url = ObsRustFeedUrl(stable.UTF8String, OBS_RUST_NIGHTLY_BRANCH);
	return [NSString stringWithUTF8String:url.c_str()];
}

- (void)dealloc
{
	@autoreleasepool {
		[_updaterController.updater removeObserver:self
						forKeyPath:NSStringFromSelector(@selector(canCheckForUpdates))];
	}
}

@end
