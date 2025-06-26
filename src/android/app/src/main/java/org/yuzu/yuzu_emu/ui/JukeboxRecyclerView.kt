// SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

package org.yuzu.yuzu_emu.ui

import android.content.Context
import android.graphics.Rect
import android.util.AttributeSet
import android.view.View
import android.view.KeyEvent
import androidx.recyclerview.widget.LinearLayoutManager
import androidx.recyclerview.widget.PagerSnapHelper
import androidx.recyclerview.widget.RecyclerView
import kotlin.math.abs
import org.yuzu.yuzu_emu.R

/**
 * JukeboxRecyclerView encapsulates all carousel/grid/list logic for the games UI.
 * It manages overlapping cards, center snapping, custom drawing order, and mid-screen swipe-to-refresh.
 * Use setCarouselMode(enabled, overlapPx) to toggle carousel features.
 */
class JukeboxRecyclerView @JvmOverloads constructor(
    context: Context,
    attrs: AttributeSet? = null,
    defStyle: Int = 0
) : RecyclerView(context, attrs, defStyle) {

    // Carousel/overlap/snap state
    private var overlapPx: Int = 0
    private var overlapDecoration: OverlappingDecoration? = null
    private var pagerSnapHelper: PagerSnapHelper? = null

    var flingMultiplier: Float = resources.getFraction(R.fraction.carousel_fling_multiplier, 1, 1)

    var useCustomDrawingOrder: Boolean = false
        set(value) {
            field = value
            setChildrenDrawingOrderEnabled(value)
            invalidate()
        }

    init {
        setChildrenDrawingOrderEnabled(true)
    }

    /**
     * Returns the horizontal center given width and paddings.
     */
    private fun calculateCenter(width: Int, paddingStart: Int, paddingEnd: Int): Int {
        return paddingStart + (width - paddingStart - paddingEnd) / 2
    }

    /**
     * Returns the horizontal center of this RecyclerView, accounting for padding.
     */
    private fun getRecyclerViewCenter(): Float {
        return calculateCenter(width, paddingLeft, paddingRight).toFloat()
    }

    /**
     * Returns the horizontal center of a LayoutManager, accounting for padding.
     */
    private fun getLayoutManagerCenter(layoutManager: RecyclerView.LayoutManager): Int {
        return if (layoutManager is LinearLayoutManager) {
            calculateCenter(layoutManager.width, layoutManager.paddingStart, layoutManager.paddingEnd)
        } else {
            width / 2
        }
    }

    private fun updateChildScalesAndAlpha() {
        val center = getRecyclerViewCenter()

        for (i in 0 until childCount) {
            val child = getChildAt(i)
            val childCenter = (child.left + child.right) / 2f
            val distance = abs(center - childCenter)
            val minScale = resources.getFraction(R.fraction.carousel_min_scale, 1, 1)
            val scale = minScale + (1f - minScale) * (1f - distance / center).coerceAtMost(1f)
            child.scaleX = scale
            child.scaleY = scale

            val maxDistance = width / 2f
            val norm = (distance / maxDistance).coerceIn(0f, 1f)
            val minAlpha = resources.getFraction(R.fraction.carousel_min_alpha, 1, 1)
            val alpha = minAlpha + (1f - minAlpha) * kotlin.math.cos(norm * Math.PI).toFloat()
            child.alpha = alpha
        }
    }

    /**
     * Enable or disable carousel mode.
     * When enabled, applies overlap, snap, and custom drawing order.
     */
    fun setCarouselMode(enabled: Boolean, overlapPx: Int = 0, cardSize: Int = 0) {
        this.overlapPx = overlapPx
        if (enabled) {
            // Add overlap decoration if not present
            if (overlapDecoration == null) {
                overlapDecoration = OverlappingDecoration(overlapPx)
                addItemDecoration(overlapDecoration!!)
            }
            // Attach PagerSnapHelper
            if (pagerSnapHelper == null) {
                pagerSnapHelper = CenterPagerSnapHelper()
                pagerSnapHelper!!.attachToRecyclerView(this)
            }
            useCustomDrawingOrder = true
            flingMultiplier = resources.getFraction(R.fraction.carousel_fling_multiplier, 1, 1)

            // Center first/last card
            post {
                if (cardSize > 0) {
                    val sidePadding = (width - cardSize) / 2
                    setPadding(sidePadding, 0, sidePadding, 0)
                    clipToPadding = false
                }
            }
            // Handle bottom insets for keyboard/navigation bar only
            androidx.core.view.ViewCompat.setOnApplyWindowInsetsListener(this) { view, insets ->
                val imeInset = insets.getInsets(androidx.core.view.WindowInsetsCompat.Type.ime()).bottom
                val navInset = insets.getInsets(androidx.core.view.WindowInsetsCompat.Type.navigationBars()).bottom
                // Only adjust bottom padding, keep top at 0
                view.setPadding(view.paddingLeft, 0, view.paddingRight, maxOf(imeInset, navInset))
                insets
            }
        } else {
            // Remove overlap decoration
            overlapDecoration?.let { removeItemDecoration(it) }
            overlapDecoration = null
            // Detach PagerSnapHelper
            pagerSnapHelper?.attachToRecyclerView(null)
            pagerSnapHelper = null
            useCustomDrawingOrder = false
            // Reset padding and fling
            setPadding(0, 0, 0, 0)
            clipToPadding = true
            flingMultiplier = 1.0f
            // Reset scaling
            for (i in 0 until childCount) {
                val child = getChildAt(i)
                child?.scaleX = 1f
                child?.scaleY = 1f
            }
        }
    }

    // trap past boundaries navigation
    override fun focusSearch(focused: View, direction: Int): View? {
        val lm = layoutManager as? LinearLayoutManager ?: return super.focusSearch(focused, direction)
        val vh = findContainingViewHolder(focused) ?: return super.focusSearch(focused, direction)
        val position = vh.bindingAdapterPosition
        val itemCount = adapter?.itemCount ?: return super.focusSearch(focused, direction)

        return when (direction) {
            View.FOCUS_LEFT -> {
                if (position > 0) {
                    findViewHolderForAdapterPosition(position - 1)?.itemView ?: super.focusSearch(focused, direction)
                } else {
                    focused
                }
            }
            View.FOCUS_RIGHT -> {
                if (position < itemCount - 1) {
                    findViewHolderForAdapterPosition(position + 1)?.itemView ?: super.focusSearch(focused, direction)
                } else {
                    focused
                }
            }
            else -> super.focusSearch(focused, direction)
        }
    }

    // Custom fling multiplier for carousel
    override fun fling(velocityX: Int, velocityY: Int): Boolean {
        val newVelocityX = (velocityX * flingMultiplier).toInt()
        val newVelocityY = (velocityY * flingMultiplier).toInt()
        return super.fling(newVelocityX, newVelocityY)
    }

    private var scaleUpdatePosted = false
    // Custom drawing order for carousel (for alpha fade)
    override fun getChildDrawingOrder(childCount: Int, i: Int): Int {
        if (!useCustomDrawingOrder || childCount == 0) return i
        val center = getRecyclerViewCenter()
        val children = (0 until childCount).map { idx ->
            val child = getChildAt(idx)
            val childCenter = (child.left + child.right) / 2f
            val distance = abs(childCenter - center)
            Pair(idx, distance)
        }
        val sorted = children.sortedWith(
            compareByDescending<Pair<Int, Float>> { it.second }
                .thenBy { it.first }
        )
        // Post scale update once per frame
        if (!scaleUpdatePosted && i == childCount - 1) {
            scaleUpdatePosted = true
            post {
                updateChildScalesAndAlpha()
                scaleUpdatePosted = false
            }
        }
        //Log.d("JukeboxRecyclerView", "Child $i got order ${sorted[i].first} at distance ${sorted[i].second} from center $center")
        return sorted[i].first
    }

    // --- OverlappingDecoration (inner class) ---
    inner class OverlappingDecoration(private val overlapPx: Int) : ItemDecoration() {
        override fun getItemOffsets(
            outRect: Rect, view: View, parent: RecyclerView, state: State
        ) {
            val position = parent.getChildAdapterPosition(view)
            if (position > 0) {
                outRect.left = -overlapPx
            }
        }
    }

    // Enable proper center snapping
    inner class CenterPagerSnapHelper : PagerSnapHelper() {

        // NEEDED: fixes center snapping, but introduces ghost movement
        override fun findSnapView(layoutManager: RecyclerView.LayoutManager): View? {
            if (layoutManager !is LinearLayoutManager) return null
            val center = (this@JukeboxRecyclerView).getLayoutManagerCenter(layoutManager)
            var minDistance = Int.MAX_VALUE
            var closestChild: View? = null
            for (i in 0 until layoutManager.childCount) {
                val child = layoutManager.getChildAt(i) ?: continue
                val childCenter = (child.left + child.right) / 2
                val distance = kotlin.math.abs(childCenter - center)
                if (distance < minDistance) {
                    minDistance = distance
                    closestChild = child
                }
            }
            return closestChild
        }

        //NEEDED: fixes ghost movement when snapping, but breaks inertial scrolling
        override fun calculateDistanceToFinalSnap(
            layoutManager: RecyclerView.LayoutManager,
            targetView: View
        ): IntArray? {
            if (layoutManager !is LinearLayoutManager) return super.calculateDistanceToFinalSnap(layoutManager, targetView)
            val out = IntArray(2)
            val center = (this@JukeboxRecyclerView).getLayoutManagerCenter(layoutManager)
            val childCenter = (targetView.left + targetView.right) / 2
            out[0] = childCenter - center
            out[1] = 0
            return out
        }

        // NEEDED: fixes inertial scrolling (broken by calculateDistanceToFinalSnap)
        override fun findTargetSnapPosition(
            layoutManager: RecyclerView.LayoutManager,
            velocityX: Int,
            velocityY: Int
        ): Int {
            if (layoutManager !is LinearLayoutManager) return RecyclerView.NO_POSITION
            val firstVisible = layoutManager.findFirstVisibleItemPosition()
            val lastVisible = layoutManager.findLastVisibleItemPosition()
            val center = (this@JukeboxRecyclerView).getLayoutManagerCenter(layoutManager)

            var closestChild: View? = null
            var minDistance = Int.MAX_VALUE
            var closestPosition = RecyclerView.NO_POSITION
            for (i in firstVisible..lastVisible) {
                val child = layoutManager.findViewByPosition(i) ?: continue
                val childCenter = (child.left + child.right) / 2
                val distance = kotlin.math.abs(childCenter - center)
                if (distance < minDistance) {
                    minDistance = distance
                    closestChild = child
                    closestPosition = i
                }
            }

            val flingCount = if (velocityX == 0) 0 else velocityX / 2000
            var targetPos = closestPosition + flingCount
            val itemCount = layoutManager.itemCount
            targetPos = targetPos.coerceIn(0, itemCount - 1)
            return targetPos
        }
    }
}