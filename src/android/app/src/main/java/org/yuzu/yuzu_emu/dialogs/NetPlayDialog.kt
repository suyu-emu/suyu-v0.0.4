// SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

package org.yuzu.yuzu_emu.dialogs

import android.annotation.SuppressLint
import android.content.Context
import android.content.res.Configuration
import android.os.Bundle
import android.os.Handler
import android.os.Looper
import android.text.Editable
import android.text.TextWatcher
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import android.widget.ArrayAdapter
import android.widget.Button
import android.widget.PopupMenu
import android.widget.Toast
import androidx.recyclerview.widget.LinearLayoutManager
import androidx.recyclerview.widget.RecyclerView
import com.google.android.material.bottomsheet.BottomSheetBehavior
import com.google.android.material.bottomsheet.BottomSheetDialog
import com.google.android.material.dialog.MaterialAlertDialogBuilder
import com.google.android.material.textfield.TextInputLayout
import org.yuzu.yuzu_emu.R
import org.yuzu.yuzu_emu.YuzuApplication
import org.yuzu.yuzu_emu.databinding.DialogMultiplayerConnectBinding
import org.yuzu.yuzu_emu.databinding.DialogMultiplayerLobbyBinding
import org.yuzu.yuzu_emu.databinding.DialogMultiplayerRoomBinding
import org.yuzu.yuzu_emu.databinding.ItemBanListBinding
import org.yuzu.yuzu_emu.databinding.ItemButtonNetplayBinding
import org.yuzu.yuzu_emu.databinding.ItemTextNetplayBinding
import org.yuzu.yuzu_emu.features.settings.model.StringSetting
import org.yuzu.yuzu_emu.network.NetDataValidators
import org.yuzu.yuzu_emu.network.NetPlayManager
import org.yuzu.yuzu_emu.utils.CompatUtils
import org.yuzu.yuzu_emu.utils.GameHelper

class NetPlayDialog(context: Context) : BottomSheetDialog(context) {
    private lateinit var adapter: NetPlayAdapter

    private val gameNameList: MutableList<Array<String>> = mutableListOf()
    private val gameIdList: MutableList<Array<Long>> = mutableListOf()

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        behavior.state = BottomSheetBehavior.STATE_EXPANDED
        behavior.state = BottomSheetBehavior.STATE_EXPANDED
        behavior.skipCollapsed =
            context.resources.configuration.orientation == Configuration.ORIENTATION_LANDSCAPE

        when {
            NetPlayManager.netPlayIsJoined() -> DialogMultiplayerLobbyBinding.inflate(
                layoutInflater
            )
                .apply {
                    setContentView(root)
                    adapter = NetPlayAdapter()
                    listMultiplayer.layoutManager = LinearLayoutManager(context)
                    listMultiplayer.adapter = adapter
                    adapter.loadMultiplayerMenu()
                    btnLeave.setOnClickListener {
                        NetPlayManager.netPlayLeaveRoom()
                        dismiss()
                    }
                    btnChat.setOnClickListener {
                        ChatDialog(context).show()
                    }

                    refreshAdapterItems()

                    btnModeration.visibility =
                        if (NetPlayManager.netPlayIsModerator()) View.VISIBLE else View.GONE
                    btnModeration.setOnClickListener {
                        showModerationDialog()
                    }
                }

            else -> {
                DialogMultiplayerConnectBinding.inflate(layoutInflater).apply {
                    setContentView(root)
                    for (game in GameHelper.cachedGameList) {
                        val gameName = game.title
                        if (gameNameList.none { it[0] == gameName }) {
                            gameNameList.add(arrayOf(gameName))
                        }

                        val gameId = game.programId.toLong()
                        if (gameIdList.none { it[0] == gameId }) {
                            gameIdList.add(arrayOf(gameId))
                        }
                    }
                    btnCreate.setOnClickListener {
                        showNetPlayInputDialog(true)
                        dismiss()
                    }
                    btnJoin.setOnClickListener {
                        showNetPlayInputDialog(false)
                        dismiss()
                    }
                    btnLobbyBrowser.setOnClickListener {
                        if (!NetDataValidators.username()) {
                            Toast.makeText(
                                context,
                                R.string.multiplayer_nickname_invalid,
                                Toast.LENGTH_LONG
                            ).show()
                        } else {
                            LobbyBrowser(context).show()
                            dismiss()
                        }
                    }
                }
            }
        }
    }

    data class NetPlayItems(
        val option: Int,
        val name: String,
        val type: Int,
        val id: Int = 0
    ) {
        companion object {
            const val MULTIPLAYER_ROOM_TEXT = 1
            const val MULTIPLAYER_ROOM_MEMBER = 2
            const val MULTIPLAYER_SEPARATOR = 3
            const val MULTIPLAYER_ROOM_COUNT = 4
            const val TYPE_BUTTON = 0
            const val TYPE_TEXT = 1
            const val TYPE_SEPARATOR = 2
        }
    }

    // TODO(alekpop, crueter): Disable context menu for self and if not moderator
    inner class NetPlayAdapter : RecyclerView.Adapter<NetPlayAdapter.NetPlayViewHolder>() {
        val netPlayItems = mutableListOf<NetPlayItems>()

        abstract inner class NetPlayViewHolder(itemView: View) :
            RecyclerView.ViewHolder(itemView),
            View.OnClickListener {
            init {
                itemView.setOnClickListener(this)
            }

            abstract fun bind(item: NetPlayItems)
        }

        inner class TextViewHolder(private val binding: ItemTextNetplayBinding) :
            NetPlayViewHolder(binding.root) {
            private lateinit var netPlayItem: NetPlayItems

            override fun onClick(clicked: View) {}

            override fun bind(item: NetPlayItems) {
                netPlayItem = item
                binding.itemTextNetplayName.text = item.name
                binding.itemIcon.apply {
                    val iconRes = when (item.option) {
                        NetPlayItems.MULTIPLAYER_ROOM_TEXT -> R.drawable.ic_system
                        NetPlayItems.MULTIPLAYER_ROOM_COUNT -> R.drawable.ic_two_users
                        else -> 0
                    }
                    visibility = if (iconRes != 0) {
                        setImageResource(iconRes)
                        View.VISIBLE
                    } else {
                        View.GONE
                    }
                }
            }
        }

        inner class ButtonViewHolder(private val binding: ItemButtonNetplayBinding) :
            NetPlayViewHolder(binding.root) {
            private lateinit var netPlayItems: NetPlayItems
            private val isModerator = NetPlayManager.netPlayIsModerator()

            init {
                binding.itemButtonMore.apply {
                    visibility = View.VISIBLE
                    setOnClickListener { showPopupMenu(it) }
                }
            }

            override fun onClick(clicked: View) {}

            private fun showPopupMenu(view: View) {
                PopupMenu(view.context, view).apply {
                    menuInflater.inflate(R.menu.menu_netplay_member, menu)
                    menu.findItem(R.id.action_kick).isEnabled = isModerator &&
                        netPlayItems.name != StringSetting.WEB_USERNAME.getString()
                    menu.findItem(R.id.action_ban).isEnabled = isModerator &&
                        netPlayItems.name != StringSetting.WEB_USERNAME.getString()
                    setOnMenuItemClickListener { item ->
                        if (item.itemId == R.id.action_kick) {
                            NetPlayManager.netPlayKickUser(netPlayItems.name)
                            true
                        } else if (item.itemId == R.id.action_ban) {
                            NetPlayManager.netPlayBanUser(netPlayItems.name)
                            true
                        } else {
                            false
                        }
                    }
                    show()
                }
            }

            override fun bind(item: NetPlayItems) {
                netPlayItems = item
                binding.itemButtonNetplayName.text = netPlayItems.name
            }
        }

        fun loadMultiplayerMenu() {
            val infos = NetPlayManager.netPlayRoomInfo()
            if (infos.isNotEmpty()) {
                val roomInfo = infos[0].split("|")
                netPlayItems.add(
                    NetPlayItems(
                        NetPlayItems.MULTIPLAYER_ROOM_TEXT,
                        roomInfo[0],
                        NetPlayItems.TYPE_TEXT
                    )
                )
                netPlayItems.add(
                    NetPlayItems(
                        NetPlayItems.MULTIPLAYER_ROOM_COUNT,
                        "${infos.size - 1}/${roomInfo[1]}",
                        NetPlayItems.TYPE_TEXT
                    )
                )
                netPlayItems.add(
                    NetPlayItems(
                        NetPlayItems.MULTIPLAYER_SEPARATOR,
                        "",
                        NetPlayItems.TYPE_SEPARATOR
                    )
                )
                for (i in 1 until infos.size) {
                    netPlayItems.add(
                        NetPlayItems(
                            NetPlayItems.MULTIPLAYER_ROOM_MEMBER,
                            infos[i],
                            NetPlayItems.TYPE_BUTTON
                        )
                    )
                }
            }
        }

        override fun getItemViewType(position: Int) = netPlayItems[position].type

        override fun onCreateViewHolder(parent: ViewGroup, viewType: Int): NetPlayViewHolder {
            val inflater = LayoutInflater.from(parent.context)
            return when (viewType) {
                NetPlayItems.TYPE_TEXT -> TextViewHolder(
                    ItemTextNetplayBinding.inflate(
                        inflater,
                        parent,
                        false
                    )
                )

                NetPlayItems.TYPE_BUTTON -> ButtonViewHolder(
                    ItemButtonNetplayBinding.inflate(
                        inflater,
                        parent,
                        false
                    )
                )

                NetPlayItems.TYPE_SEPARATOR -> object : NetPlayViewHolder(
                    inflater.inflate(
                        R.layout.item_separator_netplay,
                        parent,
                        false
                    )
                ) {
                    override fun bind(item: NetPlayItems) {}
                    override fun onClick(clicked: View) {}
                }

                else -> throw IllegalStateException("Unsupported view type")
            }
        }

        override fun onBindViewHolder(holder: NetPlayViewHolder, position: Int) {
            holder.bind(netPlayItems[position])
        }

        override fun getItemCount() = netPlayItems.size
    }

    @SuppressLint("NotifyDataSetChanged")
    fun refreshAdapterItems() {
        val handler = Handler(Looper.getMainLooper())

        NetPlayManager.setOnAdapterRefreshListener() { _, _ ->
            handler.post {
                adapter.netPlayItems.clear()
                adapter.loadMultiplayerMenu()
                adapter.notifyDataSetChanged()
            }
        }
    }

    abstract class TextValidatorWatcher(
        private val btnConfirm: Button,
        private val layout: TextInputLayout,
        private val errorMessage: String
    ) : TextWatcher {
        companion object {
            val validStates: HashMap<TextInputLayout, Boolean> = hashMapOf()
        }

        abstract fun validate(s: String): Boolean

        override fun beforeTextChanged(
            s: CharSequence?,
            start: Int,
            count: Int,
            after: Int
        ) {
        }

        override fun onTextChanged(
            s: CharSequence?,
            start: Int,
            before: Int,
            count: Int
        ) {
        }

        override fun afterTextChanged(s: Editable?) {
            val input = s.toString()
            val isValid = validate(input)
            layout.isErrorEnabled = !isValid
            layout.error = if (isValid) null else errorMessage

            validStates[layout] = isValid
            btnConfirm.isEnabled = !validStates.containsValue(false)
        }
    }

    // TODO(alekpop, crueter): Properly handle getting banned (both during and in future connects)
    private fun showNetPlayInputDialog(isCreateRoom: Boolean) {
        TextValidatorWatcher.validStates.clear()
        val activity = CompatUtils.findActivity(context)
        val dialog = BottomSheetDialog(activity)

        dialog.behavior.state = BottomSheetBehavior.STATE_EXPANDED
        dialog.behavior.state = BottomSheetBehavior.STATE_EXPANDED
        dialog.behavior.skipCollapsed =
            context.resources.configuration.orientation == Configuration.ORIENTATION_LANDSCAPE

        val binding = DialogMultiplayerRoomBinding.inflate(LayoutInflater.from(activity))
        dialog.setContentView(binding.root)

        val visibilityList: List<String> = listOf(
            context.getString(R.string.multiplayer_public_visibility),
            context.getString(R.string.multiplayer_unlisted_visibility)
        )

        binding.textTitle.text = activity.getString(
            if (isCreateRoom) {
                R.string.multiplayer_create_room
            } else {
                R.string.multiplayer_join_room
            }
        )

        // setup listeners etc
        val roomNameWatcher = object : TextValidatorWatcher(
            binding.btnConfirm,
            binding.layoutRoomName,
            context.getString(
                R.string.multiplayer_room_name_error
            )
        ) {
            override fun validate(s: String): Boolean {
                return NetDataValidators.roomName(s)
            }
        }

        val preferredWatcher = object : TextValidatorWatcher(
            binding.btnConfirm,
            binding.preferredGameName,
            context.getString(R.string.multiplayer_required)
        ) {
            override fun validate(s: String): Boolean {
                return NetDataValidators.notEmpty(s)
            }
        }

        val visibilityWatcher = object : TextValidatorWatcher(
            binding.btnConfirm,
            binding.lobbyVisibility,
            context.getString(R.string.multiplayer_token_required)
        ) {
            override fun validate(s: String): Boolean {
                return NetDataValidators.roomVisibility(s, context)
            }
        }

        val ipWatcher = object : TextValidatorWatcher(
            binding.btnConfirm,
            binding.layoutIpAddress,
            context.getString(R.string.multiplayer_ip_error)
        ) {
            override fun validate(s: String): Boolean {
                return NetDataValidators.ipAddress(s)
            }
        }

        val usernameWatcher = object : TextValidatorWatcher(
            binding.btnConfirm,
            binding.layoutUsername,
            context.getString(R.string.multiplayer_username_error)
        ) {
            override fun validate(s: String): Boolean {
                return NetDataValidators.username(s)
            }
        }

        val portWatcher = object : TextValidatorWatcher(
            binding.btnConfirm,
            binding.layoutIpPort,
            context.getString(R.string.multiplayer_port_error)
        ) {
            override fun validate(s: String): Boolean {
                return NetDataValidators.port(s)
            }
        }

        if (isCreateRoom) {
            binding.roomName.addTextChangedListener(roomNameWatcher)
            binding.dropdownPreferredGameName.addTextChangedListener(preferredWatcher)
            binding.dropdownLobbyVisibility.addTextChangedListener(visibilityWatcher)

            binding.dropdownPreferredGameName.apply {
                setAdapter(
                    ArrayAdapter(
                        activity,
                        R.layout.dropdown_item,
                        gameNameList.map { it[0] }
                    )
                )
            }

            binding.dropdownLobbyVisibility.setText(
                context.getString(R.string.multiplayer_unlisted_visibility)
            )

            binding.dropdownLobbyVisibility.apply {
                setAdapter(
                    ArrayAdapter(
                        activity,
                        R.layout.dropdown_item,
                        visibilityList
                    )
                )
            }
        }

        binding.ipAddress.addTextChangedListener(ipWatcher)
        binding.ipPort.addTextChangedListener(portWatcher)
        binding.username.addTextChangedListener(usernameWatcher)

        binding.ipAddress.setText(NetPlayManager.getRoomAddress(activity))
        binding.ipPort.setText(NetPlayManager.getRoomPort(activity))
        binding.username.setText(StringSetting.WEB_USERNAME.getString())

        // manually trigger text listeners
        if (isCreateRoom) {
            roomNameWatcher.afterTextChanged(binding.roomName.text)
            preferredWatcher.afterTextChanged(binding.dropdownPreferredGameName.text)

            // It's not needed here, the watcher is called by the initial set method
            // visibilityWatcher.afterTextChanged(binding.dropdownLobbyVisibility.text)
        }

        ipWatcher.afterTextChanged(binding.ipAddress.text)
        portWatcher.afterTextChanged(binding.ipPort.text)
        usernameWatcher.afterTextChanged(binding.username.text)

        binding.preferredGameName.visibility = if (isCreateRoom) View.VISIBLE else View.GONE
        binding.lobbyVisibility.visibility = if (isCreateRoom) View.VISIBLE else View.GONE
        binding.roomName.visibility = if (isCreateRoom) View.VISIBLE else View.GONE
        binding.maxPlayersContainer.visibility = if (isCreateRoom) View.VISIBLE else View.GONE

        binding.maxPlayersLabel.text = context.getString(
            R.string.multiplayer_max_players_value,
            binding.maxPlayers.value.toInt()
        )

        binding.maxPlayers.addOnChangeListener { _, value, _ ->
            binding.maxPlayersLabel.text =
                context.getString(R.string.multiplayer_max_players_value, value.toInt())
        }

        // TODO(alekpop, crueter): Room descriptions
        // TODO(alekpop, crueter): Preview preferred games
        binding.btnConfirm.setOnClickListener {
            binding.btnConfirm.isEnabled = false
            binding.btnConfirm.text =
                activity.getString(
                    if (isCreateRoom) {
                        R.string.multiplayer_creating
                    } else {
                        R.string.multiplayer_joining
                    }
                )

            // We don't need to worry about validation because it's already been done.
            val ipAddress = binding.ipAddress.text.toString()
            val username = binding.username.text.toString()
            val portStr = binding.ipPort.text.toString()
            val password = binding.password.text.toString()
            val port = portStr.toInt()
            val roomName = binding.roomName.text.toString()
            val maxPlayers = binding.maxPlayers.value.toInt()

            val preferredGameName = binding.dropdownPreferredGameName.text.toString()
            val preferredIdx = gameNameList.indexOfFirst { it[0] == preferredGameName }
            val preferredGameId = if (preferredIdx == -1) 0 else gameIdList[preferredIdx][0]

            val visibility = binding.dropdownLobbyVisibility.text.toString()
            val isPublic = visibility == context.getString(R.string.multiplayer_public_visibility)

            Handler(Looper.getMainLooper()).post {
                val result = if (isCreateRoom) {
                    NetPlayManager.netPlayCreateRoom(
                        ipAddress,
                        port,
                        username,
                        preferredGameName,
                        preferredGameId,
                        password,
                        roomName,
                        maxPlayers,
                        isPublic
                    )
                } else {
                    NetPlayManager.netPlayJoinRoom(ipAddress, port, username, password)
                }

                if (result == 0) {
                    StringSetting.WEB_USERNAME.setString(username)
                    NetPlayManager.setRoomPort(activity, portStr)

                    if (!isCreateRoom) NetPlayManager.setRoomAddress(activity, ipAddress)

                    Toast.makeText(
                        YuzuApplication.appContext,
                        if (isCreateRoom) {
                            R.string.multiplayer_create_room_success
                        } else {
                            R.string.multiplayer_join_room_success
                        },
                        Toast.LENGTH_LONG
                    ).show()

                    dialog.dismiss()
                } else {
                    Toast.makeText(
                        activity,
                        R.string.multiplayer_could_not_connect,
                        Toast.LENGTH_LONG
                    ).show()

                    binding.btnConfirm.isEnabled = true
                    binding.btnConfirm.text = activity.getString(R.string.ok)
                }
            }
        }

        dialog.show()
    }

    private fun showModerationDialog() {
        val activity = CompatUtils.findActivity(context)
        val dialog = MaterialAlertDialogBuilder(activity)
        dialog.setTitle(R.string.multiplayer_moderation_title)

        val banList = NetPlayManager.getBanList()
        if (banList.isEmpty()) {
            dialog.setMessage(R.string.multiplayer_no_bans)
            dialog.setPositiveButton(R.string.ok, null)
            dialog.show()
            return
        }

        val view = LayoutInflater.from(context).inflate(R.layout.dialog_ban_list, null)
        val recyclerView = view.findViewById<RecyclerView>(R.id.ban_list_recycler)
        recyclerView.layoutManager = LinearLayoutManager(context)

        lateinit var adapter: BanListAdapter

        val onUnban: (String) -> Unit = { bannedItem ->
            MaterialAlertDialogBuilder(activity)
                .setTitle(R.string.multiplayer_unban_title)
                .setMessage(activity.getString(R.string.multiplayer_unban_message, bannedItem))
                .setPositiveButton(R.string.multiplayer_unban) { _, _ ->
                    NetPlayManager.netPlayUnbanUser(bannedItem)
                    adapter.removeBan(bannedItem)
                }
                .setNegativeButton(R.string.cancel, null)
                .show()
        }

        adapter = BanListAdapter(banList, onUnban)
        recyclerView.adapter = adapter

        dialog.setView(view)
        dialog.setPositiveButton(R.string.ok, null)
        dialog.show()
    }

    private class BanListAdapter(
        banList: List<String>,
        private val onUnban: (String) -> Unit
    ) : RecyclerView.Adapter<BanListAdapter.ViewHolder>() {

        private val usernameBans = banList.filter { !it.contains(".") }.toMutableList()
        private val ipBans = banList.filter { it.contains(".") }.toMutableList()

        class ViewHolder(val binding: ItemBanListBinding) : RecyclerView.ViewHolder(binding.root)

        override fun onCreateViewHolder(parent: ViewGroup, viewType: Int): ViewHolder {
            val binding = ItemBanListBinding.inflate(
                LayoutInflater.from(parent.context),
                parent,
                false
            )
            return ViewHolder(binding)
        }

        override fun onBindViewHolder(holder: ViewHolder, position: Int) {
            val isUsername = position < usernameBans.size
            val item =
                if (isUsername) usernameBans[position] else ipBans[position - usernameBans.size]

            holder.binding.apply {
                banText.text = item
                icon.setImageResource(if (isUsername) R.drawable.ic_user else R.drawable.ic_system)
                btnUnban.setOnClickListener { onUnban(item) }
            }
        }

        override fun getItemCount() = usernameBans.size + ipBans.size

        fun removeBan(bannedItem: String) {
            val position = if (bannedItem.contains(".")) {
                ipBans.indexOf(bannedItem).let { if (it >= 0) it + usernameBans.size else it }
            } else {
                usernameBans.indexOf(bannedItem)
            }

            if (position >= 0) {
                if (bannedItem.contains(".")) {
                    ipBans.remove(bannedItem)
                } else {
                    usernameBans.remove(bannedItem)
                }
                notifyItemRemoved(position)
            }
        }
    }
}
