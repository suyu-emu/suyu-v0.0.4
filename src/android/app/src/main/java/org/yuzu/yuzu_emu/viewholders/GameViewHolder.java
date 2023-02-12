package org.yuzu.yuzu_emu.viewholders;

import android.view.View;
import android.widget.ImageView;
import android.widget.TextView;

import androidx.recyclerview.widget.RecyclerView;

import org.yuzu.yuzu_emu.R;

/**
 * A simple class that stores references to views so that the GameAdapter doesn't need to
 * keep calling findViewById(), which is expensive.
 */
public class GameViewHolder extends RecyclerView.ViewHolder {
    private View itemView;
    public ImageView imageIcon;
    public TextView textGameTitle;
    public TextView textGameCaption;

    public String gameId;

    // TODO Not need any of this stuff. Currently only the properties dialog needs it.
    public String path;
    public String title;
    public String description;
    public String regions;
    public String company;

    public GameViewHolder(View itemView) {
        super(itemView);

        this.itemView = itemView;
        itemView.setTag(this);

        imageIcon = itemView.findViewById(R.id.image_game_screen);
        textGameTitle = itemView.findViewById(R.id.text_game_title);
        textGameCaption = itemView.findViewById(R.id.text_game_caption);
    }

    public View getItemView() {
        return itemView;
    }
}
