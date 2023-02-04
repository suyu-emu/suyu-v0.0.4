package org.yuzu.yuzu_emu.ui.main;

import android.content.Intent;
import android.net.Uri;
import android.os.Bundle;
import android.view.Menu;
import android.view.MenuInflater;
import android.view.MenuItem;
import android.widget.Toast;

import androidx.annotation.NonNull;
import androidx.appcompat.app.AppCompatActivity;
import androidx.appcompat.widget.Toolbar;

import org.yuzu.yuzu_emu.NativeLibrary;
import org.yuzu.yuzu_emu.R;
import org.yuzu.yuzu_emu.activities.EmulationActivity;
import org.yuzu.yuzu_emu.features.settings.ui.SettingsActivity;
import org.yuzu.yuzu_emu.model.GameProvider;
import org.yuzu.yuzu_emu.ui.platform.PlatformGamesFragment;
import org.yuzu.yuzu_emu.utils.AddDirectoryHelper;
import org.yuzu.yuzu_emu.utils.DirectoryInitialization;
import org.yuzu.yuzu_emu.utils.FileBrowserHelper;
import org.yuzu.yuzu_emu.utils.FileUtil;
import org.yuzu.yuzu_emu.utils.PicassoUtils;
import org.yuzu.yuzu_emu.utils.StartupHandler;
import org.yuzu.yuzu_emu.utils.ThemeUtil;

/**
 * The main Activity of the Lollipop style UI. Manages several PlatformGamesFragments, which
 * individually display a grid of available games for each Fragment, in a tabbed layout.
 */
public final class MainActivity extends AppCompatActivity implements MainView {
    private Toolbar mToolbar;
    private int mFrameLayoutId;
    private PlatformGamesFragment mPlatformGamesFragment;

    private MainPresenter mPresenter = new MainPresenter(this);

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        ThemeUtil.applyTheme();

        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);

        findViews();

        setSupportActionBar(mToolbar);

        mFrameLayoutId = R.id.games_platform_frame;
        mPresenter.onCreate();

        if (savedInstanceState == null) {
            StartupHandler.handleInit(this);
            mPlatformGamesFragment = new PlatformGamesFragment();
            getSupportFragmentManager().beginTransaction().add(mFrameLayoutId, mPlatformGamesFragment).commit();
        } else {
            mPlatformGamesFragment = (PlatformGamesFragment) getSupportFragmentManager().getFragment(savedInstanceState, "mPlatformGamesFragment");
        }
        PicassoUtils.init();

        // Dismiss previous notifications (should not happen unless a crash occurred)
        EmulationActivity.tryDismissRunningNotification(this);
    }

    @Override
    protected void onSaveInstanceState(@NonNull Bundle outState) {
        super.onSaveInstanceState(outState);
        if (getSupportFragmentManager() == null) {
            return;
        }
        if (outState == null) {
            return;
        }
        getSupportFragmentManager().putFragment(outState, "mPlatformGamesFragment", mPlatformGamesFragment);
    }

    @Override
    protected void onResume() {
        super.onResume();
        mPresenter.addDirIfNeeded(new AddDirectoryHelper(this));
    }

    // TODO: Replace with a ButterKnife injection.
    private void findViews() {
        mToolbar = findViewById(R.id.toolbar_main);
    }

    @Override
    public boolean onCreateOptionsMenu(Menu menu) {
        MenuInflater inflater = getMenuInflater();
        inflater.inflate(R.menu.menu_game_grid, menu);

        return true;
    }

    /**
     * MainView
     */

    @Override
    public void setVersionString(String version) {
        mToolbar.setSubtitle(version);
    }

    @Override
    public void refresh() {
        getContentResolver().insert(GameProvider.URI_REFRESH, null);
        refreshFragment();
    }

    @Override
    public void launchSettingsActivity(String menuTag) {
        SettingsActivity.launch(this, menuTag, "");
    }

    @Override
    public void launchFileListActivity(int request) {
        switch (request) {
            case MainPresenter.REQUEST_ADD_DIRECTORY:
                FileBrowserHelper.openDirectoryPicker(this,
                        MainPresenter.REQUEST_ADD_DIRECTORY,
                        R.string.select_game_folder);
                break;
            case MainPresenter.REQUEST_INSTALL_KEYS:
                FileBrowserHelper.openFilePicker(this,
                        MainPresenter.REQUEST_INSTALL_KEYS,
                        R.string.install_keys);
                break;
        }
    }

    /**
     * @param requestCode An int describing whether the Activity that is returning did so successfully.
     * @param resultCode  An int describing what Activity is giving us this callback.
     * @param result      The information the returning Activity is providing us.
     */
    @Override
    protected void onActivityResult(int requestCode, int resultCode, Intent result) {
        super.onActivityResult(requestCode, resultCode, result);
        switch (requestCode) {
            case MainPresenter.REQUEST_ADD_DIRECTORY:
                if (resultCode == MainActivity.RESULT_OK) {
                    int takeFlags = (Intent.FLAG_GRANT_WRITE_URI_PERMISSION | Intent.FLAG_GRANT_READ_URI_PERMISSION);
                    getContentResolver().takePersistableUriPermission(Uri.parse(result.getDataString()), takeFlags);
                    // When a new directory is picked, we currently will reset the existing games
                    // database. This effectively means that only one game directory is supported.
                    // TODO(bunnei): Consider fixing this in the future, or removing code for this.
                    getContentResolver().insert(GameProvider.URI_RESET, null);
                    // Add the new directory
                    mPresenter.onDirectorySelected(FileBrowserHelper.getSelectedDirectory(result));
                }
                break;

            case MainPresenter.REQUEST_INSTALL_KEYS:
                if (resultCode == MainActivity.RESULT_OK) {
                    int takeFlags = (Intent.FLAG_GRANT_WRITE_URI_PERMISSION | Intent.FLAG_GRANT_READ_URI_PERMISSION);
                    getContentResolver().takePersistableUriPermission(Uri.parse(result.getDataString()), takeFlags);
                    String dstPath = DirectoryInitialization.getUserDirectory() + "/keys/";
                    if (FileUtil.copyUriToInternalStorage(this, result.getData(), dstPath, "prod.keys")) {
                        if (NativeLibrary.ReloadKeys()) {
                            Toast.makeText(this, R.string.install_keys_success, Toast.LENGTH_SHORT).show();
                        } else {
                            Toast.makeText(this, R.string.install_keys_failure, Toast.LENGTH_SHORT).show();
                            launchFileListActivity(MainPresenter.REQUEST_INSTALL_KEYS);
                        }
                    }
                }
                break;
        }
    }

    /**
     * Called by the framework whenever any actionbar/toolbar icon is clicked.
     *
     * @param item The icon that was clicked on.
     * @return True if the event was handled, false to bubble it up to the OS.
     */
    @Override
    public boolean onOptionsItemSelected(MenuItem item) {
        return mPresenter.handleOptionSelection(item.getItemId());
    }

    private void refreshFragment() {
        if (mPlatformGamesFragment != null) {
            mPlatformGamesFragment.refresh();
        }
    }

    @Override
    protected void onDestroy() {
        EmulationActivity.tryDismissRunningNotification(this);
        super.onDestroy();
    }
}
