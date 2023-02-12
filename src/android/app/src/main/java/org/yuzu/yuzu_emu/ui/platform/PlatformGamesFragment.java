package org.yuzu.yuzu_emu.ui.platform;

import android.database.Cursor;
import android.os.Bundle;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.view.ViewTreeObserver;
import android.widget.TextView;

import androidx.core.content.ContextCompat;
import androidx.fragment.app.Fragment;
import androidx.recyclerview.widget.GridLayoutManager;
import androidx.recyclerview.widget.RecyclerView;
import androidx.swiperefreshlayout.widget.SwipeRefreshLayout;

import org.yuzu.yuzu_emu.NativeLibrary;
import org.yuzu.yuzu_emu.YuzuApplication;
import org.yuzu.yuzu_emu.R;
import org.yuzu.yuzu_emu.adapters.GameAdapter;
import org.yuzu.yuzu_emu.model.GameDatabase;

public final class PlatformGamesFragment extends Fragment implements PlatformGamesView {
    private PlatformGamesPresenter mPresenter = new PlatformGamesPresenter(this);

    private GameAdapter mAdapter;
    private RecyclerView mRecyclerView;
    private TextView mTextView;

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
    }

    @Override
    public View onCreateView(LayoutInflater inflater, ViewGroup container, Bundle savedInstanceState) {
        View rootView = inflater.inflate(R.layout.fragment_grid, container, false);

        findViews(rootView);

        mPresenter.onCreateView();

        return rootView;
    }

    @Override
    public void onViewCreated(View view, Bundle savedInstanceState) {
        mAdapter = new GameAdapter();

        // Organize our grid layout based on the current view.
        if (isAdded()) {
            view.getViewTreeObserver()
                    .addOnGlobalLayoutListener(new ViewTreeObserver.OnGlobalLayoutListener() {
                        @Override
                        public void onGlobalLayout() {
                            if (view.getMeasuredWidth() == 0) {
                                return;
                            }

                            int columns = view.getMeasuredWidth() /
                                    requireContext().getResources().getDimensionPixelSize(R.dimen.card_width);
                            if (columns == 0) {
                                columns = 1;
                            }
                            view.getViewTreeObserver().removeOnGlobalLayoutListener(this);
                            GridLayoutManager layoutManager = new GridLayoutManager(getActivity(), columns);
                            mRecyclerView.setLayoutManager(layoutManager);
                            mRecyclerView.setAdapter(mAdapter);
                        }
                    });
        }

        // Add swipe down to refresh gesture
        final SwipeRefreshLayout pullToRefresh = view.findViewById(R.id.swipe_refresh);
        pullToRefresh.setOnRefreshListener(() -> {
            refresh();
            pullToRefresh.setRefreshing(false);
        });
    }

    @Override
    public void refresh() {
        GameDatabase databaseHelper = YuzuApplication.databaseHelper;
        databaseHelper.scanLibrary(databaseHelper.getWritableDatabase());
        mPresenter.refresh();
        updateTextView();
    }

    @Override
    public void showGames(Cursor games) {
        if (mAdapter != null) {
            mAdapter.swapCursor(games);
        }
        updateTextView();
    }

    private void updateTextView() {
        mTextView.setVisibility(mAdapter.getItemCount() == 0 ? View.VISIBLE : View.GONE);
    }

    private void findViews(View root) {
        mRecyclerView = root.findViewById(R.id.grid_games);
        mTextView = root.findViewById(R.id.gamelist_empty_text);
    }
}
