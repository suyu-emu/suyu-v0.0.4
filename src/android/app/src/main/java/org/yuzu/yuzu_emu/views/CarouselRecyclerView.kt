// SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

package org.yuzu.yuzu_emu.ui

import android.content.Context
import android.graphics.Rect
import android.util.AttributeSet
import android.view.View
import androidx.recyclerview.widget.LinearLayoutManager
import androidx.recyclerview.widget.PagerSnapHelper
import androidx.recyclerview.widget.RecyclerView
import kotlin.math.abs
import org.yuzu.yuzu_emu.R
import org.yuzu.yuzu_emu.adapters.GameAdapter
import androidx.core.view.doOnNextLayout
import org.yuzu.yuzu_emu.YuzuApplication
import androidx.preference.PreferenceManager

/**
 * CarouselRecyclerView encapsulates all carousel logic for the games UI.
 * It manages overlapping cards, center snapping, custom drawing order,
 * joypad & fling navigation and mid-screen swipe-to-refresh.
 */
class CarouselRecyclerView @JvmOverloads constructor(
    context: Context,
    attrs: AttributeSet? = null,
    defStyle: Int = 0
) : RecyclerView(context, attrs, defStyle) {

    private var overlapFactor: Float = 0f
    private var overlapPx: Int = 0
    private var overlapDecoration: OverlappingDecoration? = null
    private var pagerSnapHelper: PagerSnapHelper? = null
    private var scalingScrollListener: OnScrollListener? = null

    companion object {
        private const val CAROUSEL_CARD_SIZE_FACTOR = "CarouselCardSizeMultiplier"
        private const val CAROUSEL_BORDERCARDS_SCALE = "CarouselBorderCardsScale"
        private const val CAROUSEL_BORDERCARDS_ALPHA = "CarouselBorderCardsAlpha"
        private const val CAROUSEL_OVERLAP_FACTOR = "CarouselOverlapFactor"
        private const val CAROUSEL_MAX_FLING_COUNT = "CarouselMaxFlingCount"
        private const val CAROUSEL_FLING_MULTIPLIER = "CarouselFlingMultiplier"
        private const val CAROUSEL_CARDS_SCALING_SHAPE = "CarouselCardsScalingShape"
        private const val CAROUSEL_CARDS_ALPHA_SHAPE = "CarouselCardsAlphaShape"
        const val CAROUSEL_LAST_SCROLL_POSITION = "CarouselLastScrollPosition"
        const val CAROUSEL_VIEW_TYPE_PORTRAIT = "GamesViewTypePortrait"
        const val CAROUSEL_VIEW_TYPE_LANDSCAPE = "GamesViewTypeLandscape"
    }

    private val preferences =
        PreferenceManager.getDefaultSharedPreferences(YuzuApplication.appContext)

    var flingMultiplier: Float = 1f

    var pendingScrollAfterReload: Boolean = false

    var useCustomDrawingOrder: Boolean = false
        set(value) {
            field = value
            setChildrenDrawingOrderEnabled(value)
            invalidate()
        }

    init {
        setChildrenDrawingOrderEnabled(true)
    }

    private fun calculateCenter(width: Int, paddingStart: Int, paddingEnd: Int): Int {
        return paddingStart + (width - paddingStart - paddingEnd) / 2
    }

    private fun getRecyclerViewCenter(): Float {
        return calculateCenter(width, paddingLeft, paddingRight).toFloat()
    }

    private fun getLayoutManagerCenter(layoutManager: RecyclerView.LayoutManager): Int {
        return if (layoutManager is LinearLayoutManager) {
            calculateCenter(
                layoutManager.width,
                layoutManager.paddingStart,
                layoutManager.paddingEnd
            )
        } else {
            width / 2
        }
    }

    private fun getChildDistanceToCenter(view: View): Float {
        return 0.5f * (view.left + view.right) - getRecyclerViewCenter()
    }

    fun restoreScrollState(position: Int = 0, attempts: Int = 0) {
        val lm = layoutManager as? LinearLayoutManager ?: return
        if (lm.findLastVisibleItemPosition() == RecyclerView.NO_POSITION && attempts < 10) {
            post { restoreScrollState(position, attempts + 1) }
            return
        }
        scrollToPosition(position)
    }

    fun getClosestChildPosition(fullRange: Boolean = false): Int {
        val lm = layoutManager as? LinearLayoutManager ?: return RecyclerView.NO_POSITION
        var minDistance = Int.MAX_VALUE
        var closestPosition = RecyclerView.NO_POSITION
        val start = if (fullRange) 0 else lm.findFirstVisibleItemPosition()
        val end = if (fullRange) lm.childCount - 1 else lm.findLastVisibleItemPosition()
        for (i in start..end) {
            val child = lm.findViewByPosition(i) ?: continue
            val distance = kotlin.math.abs(getChildDistanceToCenter(child).toInt())
            if (distance < minDistance) {
                minDistance = distance
                closestPosition = i
            }
        }
        return closestPosition
    }

    fun updateChildScalesAndAlpha() {
        for (i in 0 until childCount) {
            val child = getChildAt(i) ?: continue
            updateChildScaleAndAlphaForPosition(child)
        }
    }

    fun shapingFunction(x: Float, option: Int = 0): Float {
        return when (option) {
            0 -> 1f // Off
            1 -> 1f - x // linear descending
            2 -> (1f - x) * (1f - x) // Ease out
            3 -> if (x < 0.05f) 1f else (1f - x) * 0.8f
            4 -> kotlin.math.cos(x * Math.PI).toFloat() // Cosine
            5 -> kotlin.math.cos((1.5f * x).coerceIn(0f, 1f) * Math.PI).toFloat() // Cosine 1.5x trimmed
            else -> 1f // Default to Off
        }
    }

    fun updateChildScaleAndAlphaForPosition(child: View) {
        val cardSize = (adapter as? GameAdapter ?: return).cardSize
        val position = getChildViewHolder(child).bindingAdapterPosition
        if (position == RecyclerView.NO_POSITION || cardSize <= 0) {
            return // No valid position or card size
        }
        child.layoutParams.width = cardSize
        child.layoutParams.height = cardSize

        val center = getRecyclerViewCenter()
        val distance = abs(getChildDistanceToCenter(child))
        val internalBorderScale = resources.getFraction(R.fraction.carousel_bordercards_scale, 1, 1)
        val borderScale = preferences.getFloat(CAROUSEL_BORDERCARDS_SCALE, internalBorderScale).coerceIn(
            0f,
            1f
        )

        val shapeInput = (distance / center).coerceIn(0f, 1f)
        val internalShapeSetting = resources.getInteger(R.integer.carousel_cards_scaling_shape)
        val scalingShapeSetting = preferences.getInt(
            CAROUSEL_CARDS_SCALING_SHAPE,
            internalShapeSetting
        )
        val shapedScaling = shapingFunction(shapeInput, scalingShapeSetting)
        val scale = (borderScale + (1f - borderScale) * shapedScaling).coerceIn(0f, 1f)

        val maxDistance = width / 2f
        val alphaInput = (distance / maxDistance).coerceIn(0f, 1f)
        val internalBordersAlpha = resources.getFraction(
            R.fraction.carousel_bordercards_alpha,
            1,
            1
        )
        val borderAlpha = preferences.getFloat(CAROUSEL_BORDERCARDS_ALPHA, internalBordersAlpha).coerceIn(
            0f,
            1f
        )
        val internalAlphaShapeSetting = resources.getInteger(R.integer.carousel_cards_alpha_shape)
        val alphaShapeSetting = preferences.getInt(
            CAROUSEL_CARDS_ALPHA_SHAPE,
            internalAlphaShapeSetting
        )
        val shapedAlpha = shapingFunction(alphaInput, alphaShapeSetting)
        val alpha = (borderAlpha + (1f - borderAlpha) * shapedAlpha).coerceIn(0f, 1f)

        child.animate().cancel()
        child.alpha = alpha
        child.scaleX = scale
        child.scaleY = scale
    }

    fun focusCenteredCard() {
        val centeredPos = getClosestChildPosition()
        if (centeredPos != RecyclerView.NO_POSITION) {
            val vh = findViewHolderForAdapterPosition(centeredPos)
            vh?.itemView?.let { child ->
                child.isFocusable = true
                child.isFocusableInTouchMode = true
                child.requestFocus()
            }
        }
    }

    fun setCarouselMode(enabled: Boolean, gameAdapter: GameAdapter? = null) {
        if (enabled) {
            useCustomDrawingOrder = true

            val insets = rootWindowInsets
            val bottomInset = insets?.getInsets(android.view.WindowInsets.Type.systemBars())?.bottom ?: 0
            val internalFactor = resources.getFraction(R.fraction.carousel_card_size_factor, 1, 1)
            val userFactor = preferences.getFloat(CAROUSEL_CARD_SIZE_FACTOR, internalFactor).coerceIn(
                0f,
                1f
            )
            val cardSize = (userFactor * (height - bottomInset)).toInt()
            gameAdapter?.setCardSize(cardSize)

            val internalOverlapFactor = resources.getFraction(
                R.fraction.carousel_overlap_factor,
                1,
                1
            )
            overlapFactor = preferences.getFloat(CAROUSEL_OVERLAP_FACTOR, internalOverlapFactor).coerceIn(
                0f,
                1f
            )
            overlapPx = (cardSize * overlapFactor).toInt()

            val internalFlingMultiplier = resources.getFraction(
                R.fraction.carousel_fling_multiplier,
                1,
                1
            )
            flingMultiplier = preferences.getFloat(
                CAROUSEL_FLING_MULTIPLIER,
                internalFlingMultiplier
            ).coerceIn(1f, 5f)

            gameAdapter?.registerAdapterDataObserver(object : RecyclerView.AdapterDataObserver() {
                override fun onChanged() {
                    if (pendingScrollAfterReload) {
                        post {
                            jigglyScroll()
                            pendingScrollAfterReload = false
                        }
                    }
                }
            })

            // Detach SnapHelper during setup
            pagerSnapHelper?.attachToRecyclerView(null)

            // Add overlap decoration if not present
            if (overlapDecoration == null) {
                overlapDecoration = OverlappingDecoration(overlapPx)
                addItemDecoration(overlapDecoration!!)
            }

            // Gradual scalingAdd commentMore actions
            if (scalingScrollListener == null) {
                scalingScrollListener = object : OnScrollListener() {
                    override fun onScrolled(recyclerView: RecyclerView, dx: Int, dy: Int) {
                        super.onScrolled(recyclerView, dx, dy)
                        updateChildScalesAndAlpha()
                    }
                }
                addOnScrollListener(scalingScrollListener!!)
            }

            if (cardSize > 0) {
                val topPadding = ((height - bottomInset - cardSize) / 2).coerceAtLeast(0) // Center vertically
                val sidePadding = (width - cardSize) / 2 // Center first/last card
                setPadding(sidePadding, topPadding, sidePadding, 0)
                clipToPadding = false
            }

            if (pagerSnapHelper == null) {
                pagerSnapHelper = CenterPagerSnapHelper()
                pagerSnapHelper!!.attachToRecyclerView(this)
            }
        } else {
            // Remove overlap decoration
            overlapDecoration?.let { removeItemDecoration(it) }
            overlapDecoration = null
            // Remove scaling scroll listener
            scalingScrollListener?.let { removeOnScrollListener(it) }
            scalingScrollListener = null
            // Detach PagerSnapHelper
            pagerSnapHelper?.attachToRecyclerView(null)
            pagerSnapHelper = null
            useCustomDrawingOrder = false
            // Reset padding and fling
            setPadding(0, 0, 0, 0)
            clipToPadding = true
            flingMultiplier = 1f
            // Reset scaling
            for (i in 0 until childCount) {
                val child = getChildAt(i)
                child?.scaleX = 1f
                child?.scaleY = 1f
                child?.alpha = 1f
            }
        }
    }

    override fun onScrollStateChanged(state: Int) {
        super.onScrollStateChanged(state)
        if (state == RecyclerView.SCROLL_STATE_IDLE) {
            focusCenteredCard()
        }
    }

    override fun scrollToPosition(position: Int) {
        super.scrollToPosition(position)
        (layoutManager as? LinearLayoutManager)?.scrollToPositionWithOffset(position, overlapPx)
        doOnNextLayout {
            updateChildScalesAndAlpha()
            focusCenteredCard()
        }
    }

    private var lastFocusSearchTime: Long = 0
    override fun focusSearch(focused: View, direction: Int): View? {
        if (layoutManager !is LinearLayoutManager) return super.focusSearch(focused, direction)
        val vh = findContainingViewHolder(focused) ?: return super.focusSearch(focused, direction)
        val itemCount = adapter?.itemCount ?: return super.focusSearch(focused, direction)
        val position = vh.bindingAdapterPosition

        return when (direction) {
            View.FOCUS_LEFT -> {
                if (position > 0) {
                    val now = System.currentTimeMillis()
                    val repeatDetected = (now - lastFocusSearchTime) < resources.getInteger(
                        R.integer.carousel_focus_search_repeat_threshold_ms
                    )
                    lastFocusSearchTime = now
                    if (!repeatDetected) { // ensures the first run
                        val offset = focused.width - overlapPx
                        smoothScrollBy(-offset, 0)
                    }
                    findViewHolderForAdapterPosition(position - 1)?.itemView ?: super.focusSearch(
                        focused,
                        direction
                    )
                } else {
                    focused
                }
            }
            View.FOCUS_RIGHT -> {
                if (position < itemCount - 1) {
                    findViewHolderForAdapterPosition(position + 1)?.itemView ?: super.focusSearch(
                        focused,
                        direction
                    )
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

    // Custom drawing order for carousel (for alpha fade)
    override fun getChildDrawingOrder(childCount: Int, i: Int): Int {
        if (!useCustomDrawingOrder || childCount == 0) return i
        val children = (0 until childCount).map { idx ->
            val distance = abs(getChildDistanceToCenter(getChildAt(idx)))
            Pair(idx, distance)
        }
        val sorted = children.sortedWith(
            compareByDescending<Pair<Int, Float>> { it.second }
                .thenBy { it.first }
        )
        return sorted[i].first
    }

    fun jigglyScroll() {
        scrollBy(-1, 0)
        scrollBy(1, 0)
        focusCenteredCard()
    }

    inner class OverlappingDecoration(private val overlap: Int) : ItemDecoration() {
        override fun getItemOffsets(
            outRect: Rect,
            view: View,
            parent: RecyclerView,
            state: State
        ) {
            val position = parent.getChildAdapterPosition(view)
            if (position > 0) {
                outRect.left = -overlap
            }
        }
    }

    inner class VerticalCenterDecoration : ItemDecoration() {
        override fun getItemOffsets(
            outRect: android.graphics.Rect,
            view: View,
            parent: RecyclerView,
            state: RecyclerView.State
        ) {
            val parentHeight = parent.height
            val childHeight = view.layoutParams.height.takeIf { it > 0 }
                ?: view.measuredHeight.takeIf { it > 0 }
                ?: view.height

            if (parentHeight > 0 && childHeight > 0) {
                val verticalPadding = ((parentHeight - childHeight) / 2).coerceAtLeast(0)
                outRect.top = verticalPadding
                outRect.bottom = verticalPadding
            }
        }
    }

    inner class CenterPagerSnapHelper : PagerSnapHelper() {

        // NEEDED: fixes center snapping, but introduces ghost movement
        override fun findSnapView(layoutManager: RecyclerView.LayoutManager): View? {
            if (layoutManager !is LinearLayoutManager) return null
            return layoutManager.findViewByPosition(getClosestChildPosition())
        }

        // NEEDED: fixes ghost movement when snapping, but breaks inertial scrolling
        override fun calculateDistanceToFinalSnap(
            layoutManager: RecyclerView.LayoutManager,
            targetView: View
        ): IntArray? {
            if (layoutManager !is LinearLayoutManager) {
                return super.calculateDistanceToFinalSnap(
                    layoutManager,
                    targetView
                )
            }
            val out = IntArray(2)
            out[0] = getChildDistanceToCenter(targetView).toInt()
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
            val closestPosition = this@CarouselRecyclerView.getClosestChildPosition()
            val internalMaxFling = resources.getInteger(R.integer.carousel_max_fling_count)
            val maxFling = preferences.getInt(CAROUSEL_MAX_FLING_COUNT, internalMaxFling).coerceIn(
                1,
                10
            )
            val rawFlingCount = if (velocityX == 0) 0 else velocityX / 2000
            val flingCount = rawFlingCount.coerceIn(-maxFling, maxFling)
            val targetPos = (closestPosition + flingCount).coerceIn(0, layoutManager.itemCount - 1)
            return targetPos
        }
    }
}
