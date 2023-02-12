package org.yuzu.yuzu_emu.utils;

import android.graphics.Bitmap;
import android.graphics.BitmapFactory;

import com.squareup.picasso.Picasso;
import com.squareup.picasso.Request;
import com.squareup.picasso.RequestHandler;

import org.yuzu.yuzu_emu.NativeLibrary;

import java.nio.IntBuffer;

public class GameIconRequestHandler extends RequestHandler {
    @Override
    public boolean canHandleRequest(Request data) {
        return "content".equals(data.uri.getScheme());
    }

    @Override
    public Result load(Request request, int networkPolicy) {
        String gamePath = request.uri.toString();
        byte[] data = NativeLibrary.GetIcon(gamePath);
        BitmapFactory.Options options = new BitmapFactory.Options();
        options.inMutable = true;
        Bitmap bitmap = BitmapFactory.decodeByteArray(data, 0, data.length, options);
        return new Result(bitmap, Picasso.LoadedFrom.DISK);
    }
}
