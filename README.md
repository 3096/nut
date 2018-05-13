# Y'allAreNUTs
Some filler for 5.0.x before Checkpoint works

previously known as switch-save-dump-hack-job

Seriously, y'all just need to wait for Checkpoint...

But since I know no one would listen to me and try to use this mess anyway...

Here's an update.

Now you can use UP & DOWN, LEFT & RIGHT to select a save to dump. All dumps will be saved to `save` by pressing A or `save/{titleID}/{userID}/` by pressing Y (will overwrite whatever's already in there.) Press ZR to dump all saves. Pressing X will inject all data that's present in `inject` (sometimes doesn't work due to service being broken on 5.0 and without using it, fsdevCommitDevice() can fail... Use at your own risk and always have backups)

Build it with [my fork of libnx](https://github.com/3096/libnx) btw.

This was never meant to be something that I wanted to release, but since nobody knows how to "wait", I had to make it a bit more proper so it doesn't get too messy too quickly. Hence the name of this homebrew that wasn't meant to be: You guys are nuts!

I don't intend on spending any more time on this tool since it's quite unnecessary. If you want to make any improvements, you're welcome to create a pull request.
