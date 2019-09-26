# Download all pull requests as patches that match a specific label
# Usage: python download-patches-by-label.py <Label to Match> <Root Path Folder to DL to>

import requests, sys, json, urllib3.request, shutil, subprocess, os, traceback

org = os.getenv("PrivateMergeOrg".upper(), "yuzu-emu")
repo = os.getenv("PrivateMergeRepo".upper(), "yuzu-private")
tagline = os.getenv("MergeTaglinePrivate".upper(), "")
user = sys.argv[1]

http = urllib3.PoolManager()
dl_list = {}

def check_individual(repo_id, pr_id):
    url = 'https://%sdev.azure.com/%s/%s/_apis/git/repositories/%s/pullRequests/%s/labels?api-version=5.1-preview.1' % (user, org, repo, repo_id, pr_id)
    response = requests.get(url)
    if (response.ok):
        j = json.loads(response.content)
        for tg in j['value']:
            if (tg['name'] == sys.argv[2]):
                return True
    return False

try:
    url = 'https://%sdev.azure.com/%s/%s/_apis/git/pullrequests?api-version=5.1' % (user, org, repo)
    response = requests.get(url)
    if (response.ok):
        j = json.loads(response.content)
        for pr in j["value"]:
            repo_id = pr['repository']['id']
            pr_id = pr['pullRequestId']
            if (check_individual(repo_id, pr_id)):
                pn = pr_id
                ref  = pr['sourceRefName']
                print("Matched PR# %s" % pn)
                print(subprocess.check_output(["git", "fetch", "https://%sdev.azure.com/%s/_git/%s" % (user, org, repo), ref, "-f"]))
                print(subprocess.check_output(["git", "merge", "--squash", 'origin/' + ref.replace('refs/heads/','')]))
                print(subprocess.check_output(["git", "commit", "-m\"Merge %s PR %s\"" % (tagline, pn)]))
except:
    traceback.print_exc(file=sys.stdout)
    sys.exit(-1)
