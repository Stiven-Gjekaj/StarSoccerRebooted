package io.github.stivengjekaj.starsoccerrebooted;

import android.content.pm.ApplicationInfo;
import android.content.pm.PackageManager;

import org.libsdl.app.SDLActivity;

// The activity of the app. The native code is in libmain.so.
public class StarSoccerActivity extends SDLActivity {
    @Override
    protected String[] getLibraries() {
        return new String[] {"main"};
    }

    // The path of the APK of the installed New Star Soccer, or null when it
    // is not installed. The native code imports the assets from it.
    public static String installedGamePath() {
        try {
            PackageManager manager = getContext().getPackageManager();
            ApplicationInfo info = manager.getApplicationInfo("com.newstargames.newstarsoccer", 0);
            return info.sourceDir;
        } catch (PackageManager.NameNotFoundException e) {
            return null;
        }
    }
}
