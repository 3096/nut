# switch-save-dump-hack-job
Some nut wack hack job for 5.0.x before Checkpoint works

Seriously, y'all just need to wait for Checkpoint...

But since I know no one would listen to me and try to use this mess anyway...

Here's an update.

Now you can use up & down to select a save to dump. All dumps will be saved to `save` by pressing A (will overwrite whatever's already in there.) Pressing X will inject all data that's present in `inject` (sometimes doesn't work due to service being broken on 5.0 and without using it, fsdevCommitDevice() can fail... Use at your own risk and always have backups)

Build it with [my fork of libnx](https://github.com/3096/libnx) btw.

What have I done?
