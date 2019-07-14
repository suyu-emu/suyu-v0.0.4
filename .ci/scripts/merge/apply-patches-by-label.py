# Download all pull requests as patches that match a specific label
# Usage: python download-patches-by-label.py <Label to Match> <Root Path Folder to DL to>

import requests, sys, json, urllib3.request, shutil, subprocess

http = urllib3.PoolManager()
dl_list = {}

def check_individual(labels):
    for label in labels:
        if (label["name"] == sys.argv[1]):
            return True
    return False

try:
    url = 'https://api.github.com/repos/yuzu-emu/yuzu/pulls'
    response = requests.get(url)
    if (response.ok):
        j = json.loads(response.content)
        for pr in j:
            if (check_individual(pr["labels"])):
                pn = pr["number"]
                print("Matched PR# %s" % pn)
                print(subprocess.check_output(["git", "fetch", "https://github.com/yuzu-emu/yuzu.git", "pull/%s/head:pr-%s" % (pn, pn), "-f"]))
                print(subprocess.check_output(["git", "merge", "--squash", "pr-%s" % pn]))
                print(subprocess.check_output(["git", "commit", "-m\"Merge PR %s\"" % pn]))
except:
    sys.exit(-1)
