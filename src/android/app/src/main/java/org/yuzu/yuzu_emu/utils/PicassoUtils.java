package org.yuzu.yuzu_emu.utils;

import android.graphics.Bitmap;
import android.net.Uri;
import android.widget.ImageView;

import com.squareup.picasso.Picasso;

import org.yuzu.yuzu_emu.YuzuApplication;
import org.yuzu.yuzu_emu.R;

import java.io.IOException;

import androidx.annotation.Nullable;

public class PicassoUtils {
    private static boolean mPicassoInitialized = false;

    public static void init() {
        if (mPicassoInitialized) {
            return;
        }
        Picasso picassoInstance = new Picasso.Builder(YuzuApplication.getAppContext())
                .addRequestHandler(new GameIconRequestHandler())
                .build();

        Picasso.setSingletonInstance(picassoInstance);
        mPicassoInitialized = true;
    }

    public static void loadGameIcon(ImageView imageView, String gamePath) {
        Picasso
                .get()
                .load(Uri.parse("iso:/" + gamePath))
                .fit()
                .centerInside()
                .config(Bitmap.Config.RGB_565)
                .error(R.drawable.no_icon)
                .transform(new PicassoRoundedCornersTransformation())
                .into(imageView);
    }

    // Blocking call. Load image from file and crop/resize it to fit in width x height.
    @Nullable
    public static Bitmap LoadBitmapFromFile(String uri, int width, int height) {
        try {
            return Picasso.get()
                    .load(Uri.parse(uri))
                    .config(Bitmap.Config.ARGB_8888)
                    .centerCrop()
                    .resize(width, height)
                    .get();
        } catch (IOException e) {
            return null;
        }
    }
}
