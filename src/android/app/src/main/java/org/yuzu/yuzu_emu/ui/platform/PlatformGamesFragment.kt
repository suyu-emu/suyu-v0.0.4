package org.yuzu.yuzu_emu.ui.platform

import android.database.Cursor
import android.os.Bundle
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import android.view.ViewTreeObserver.OnGlobalLayoutListener
import android.widget.TextView
import androidx.fragment.app.Fragment
import androidx.recyclerview.widget.GridLayoutManager
import androidx.recyclerview.widget.RecyclerView
import androidx.swiperefreshlayout.widget.SwipeRefreshLayout
import org.yuzu.yuzu_emu.R
import org.yuzu.yuzu_emu.YuzuApplication
import org.yuzu.yuzu_emu.adapters.GameAdapter

class PlatformGamesFragment : Fragment(), PlatformGamesView {
    private val presenter = PlatformGamesPresenter(this)
    private var adapter: GameAdapter? = null
    private lateinit var recyclerView: RecyclerView
    private lateinit var textView: TextView

    override fun onCreateView(
        inflater: LayoutInflater,
        container: ViewGroup?,
        savedInstanceState: Bundle?
    ): View? {
        val rootView = inflater.inflate(R.layout.fragment_grid, container, false)
        findViews(rootView)
        presenter.onCreateView()
        return rootView
    }

    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        adapter = GameAdapter()

        // Organize our grid layout based on the current view.
        if (isAdded) {
            view.viewTreeObserver
                .addOnGlobalLayoutListener(object : OnGlobalLayoutListener {
                    override fun onGlobalLayout() {
                        if (view.measuredWidth == 0) {
                            return
                        }
                        var columns = view.measuredWidth /
                                requireContext().resources.getDimensionPixelSize(R.dimen.card_width)
                        if (columns == 0) {
                            columns = 1
                        }
                        view.viewTreeObserver.removeOnGlobalLayoutListener(this)
                        val layoutManager = GridLayoutManager(activity, columns)
                        recyclerView.layoutManager = layoutManager
                        recyclerView.adapter = adapter
                    }
                })
        }

        // Add swipe down to refresh gesture
        val pullToRefresh = view.findViewById<SwipeRefreshLayout>(R.id.swipe_refresh)
        pullToRefresh.setOnRefreshListener {
            refresh()
            pullToRefresh.isRefreshing = false
        }
    }

    override fun refresh() {
        val databaseHelper = YuzuApplication.databaseHelper
        databaseHelper!!.scanLibrary(databaseHelper.writableDatabase)
        presenter.refresh()
        updateTextView()
    }

    override fun showGames(games: Cursor) {
        if (adapter != null) {
            adapter!!.swapCursor(games)
        }
        updateTextView()
    }

    private fun updateTextView() {
        textView.visibility =
            if (adapter!!.itemCount == 0) View.VISIBLE else View.GONE
    }

    private fun findViews(root: View) {
        recyclerView = root.findViewById(R.id.grid_games)
        textView = root.findViewById(R.id.gamelist_empty_text)
    }

    companion object {
        const val TAG = "PlatformGamesFragment"
    }
}
