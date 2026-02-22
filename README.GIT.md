# Git Instructions for Xastir users and developers

For those who think git might be a bit too complicated to deal with,
here are (I think) the minimal commands.


# Basics for USERS:

## Getting Your Initial Copy (Git Clone):

0. Make sure git is installed on your system.

1. Run
```
     git config --global user.name "Your Name" user.email "Your@email.address"
```
The above is not strictly necessary, but if you ever try to make changes
to Xastir and get them integrated with the project it is important.

<details>
<summary>If you already have a different git config</summary>
If you already have a different git global config, you can create
a local config for a particular repo by going into that repo and doing:
git config --local user.name "Your Name" user.email "Your@email.address"
</details>

Check the config by:
```
     git config --local -l
     git config --global -l
     git config -l   # Doesn't differentiate between global and local though!
```

2. Go to <http://github.com/Xastir/Xastir> to access the project page.
  There you will find the URL of the git repository, just to the right
  of a button that says "HTTPS". Copy this URL to your clipboard.  (At
  the time of this writing, the URL was
  https://github.com/Xastir/Xastir.git)

3. Open a shell, navigate to a directory where you want to store the
Xastir source code, and enter this command:
```
    git  clone https://github.com/Xastir/Xastir.git
```

This will create a clone of the Xastir git repository in an
"Xastir" subdirectory of the current directory.


All done!  You now have the latest development sources on your
computer.  Not only that, you have a complete copy of the entire
project history and access to all prior releases.

4. Please set your default git commit message template for the project to
the one included in the Xastir source tree:

```
    cd Xastir
    cp git_commit_message_template ~
    git config --global commit.template ~/git_commit_message_template
```

This will assure that when you make commits, your editor will start
with a template that helps guide you through the process of
writing commit messages that conform to the guidelines below in
the section titled "Important: Git Commit Message Format".
This template is quite generic and conforms to the guidelines
of many other open source projects, so it is reasonable to make
it a global git option.

##  Updating Your Copy

The copy you just cloned is the snapshot of the state of the code the
day you did it.  From time to time you'll want to update that to get
the latest changes.  It's easy.

```
 cd Xastir
 git pull           # Update your local repo (May be dangerous for developers)
 ./bootstrap.sh     # "autoreconf -i" also works
 mkdir -p build     # Build in a separate directory
 cd build
 ../configure
 (make clean;make -j3 2>&1) | tee make.log
 sudo make install  # "make install-strip" can be used after the first
                    # time: It removes debugging info from executable
 sudo chmod 4555 /usr/local/bin/xastir  # Only needed if using kernel AX.25
 xastir &   # Start it up!
```

Note that you'll need autoconf 2.53 or newer and automake 1.16 or newer
in order to run the "./bootstrap.sh" script.

<details>
<summary>autoreconf vs. bootstrap</summary>
"autoreconf", a part of the autoconf package, does the same
thing that bootstrap.sh does, in a more modern approach.  The
bootstrap.sh approach is very old and was the approach advocated by
this document for many years, but invoking "autoreconf -i" does the
same thing.
</details>

<details>
<summary>It is pointless to continue if autoreconf or bootstrap.sh give you error messages!</summary>
The bootstrap.sh script or autoreconf is what creates the configure
script.  Running either requires having autoconf and automake
installed, and it will fail if these are not installed.  If
bootstrap.sh fails, configure won't exist.  If bootstrap.sh fails,
stop, fix the problem and try again.  There is no point continuing if
bootstrap.sh gives you error messages about programs not being found
(aclocal, autoconf, automake).

This is a common problem reported to the Xastir team, and its solution
is always the same:  install all prerequisite tools before trying to
build Xastir.
</details>

# Git details for DEVELOPERS:

These instructions are by necessity very, very incomplete.  Don't rely
solely on this document to learn git.  See a good resource like [Pro
Git](https://git-scm.com/book/en/v2) before getting too involved in
Xastir development.

## Initial Checkout:

Since you will be uploading changes to the repo, it is important to
get set up right.  Choose your initial clone URL accordingly.

### HTTPS Method:

```
   git clone https://github.com/Xastir/Xastir
```

### SSH public key method

[Add keys to GitHub](https://docs.github.com/en/authentication/connecting-to-github-with-ssh/adding-a-new-ssh-key-to-your-github-account) and then:

```
   git clone git@github.com:Xastir/Xastir
```

Note that using the SSH method means that you won't have to answer the
popups for user/password each time you do anything with the remote repo,
although you will have to enter a passphrase if you added a passphrase to
your SSH key. The SSH method is highly recommended for active developers!


# Normal Development Worklow:

## The Xastir team prefers a "fork and pull request" development flow

In this way of working with git, you create a fork on github (just
click the "fork" button on the Xastir github main page).  This creates
a clone in your own account on github that is entirely yours, but is
linked back to the main repo.

See [this
site](https://gist.github.com/Chaser324/ce0505fbed06b947d962) for a
good summary of how the process works.

## Create your fork

Go to Xastir's github repo, click the fork button, and make your fork
in your own github account.

## Link your local clone to your fork

When you cloned Xastir's main repo, git set up a "remote" for you that
refers back to that main repo on github.  It was called "origin" if
you didn't do anything special to rename it.

You now want to create a second remote that points at your own fork.

Navigate to your fork on github and look at the "Code" button, which
will show you the URL you would use to clone your repo.  Use this URL
in a command like the following in your Xastir clone directory:

```
git remote add myremote git@github.com:myusername/Xastir.git
```

You now have a way of working with your own fork and the main upstream
repo from the same directory.  You can confirm it's right with:
```
git remote -v
```
which should list your remotes:
```
origin  git@github.com:Xastir/Xastir.git (fetch)
origin  git@github.com:Xastir/Xastir.git (push)
myremote git@github.com:myusername/Xastir.git (fetch)
myremote git@github.com:myusername/Xastir.git (push)
```

You should then actually fetch the contents of your fork:
```
git fetch myremote
```
This really doesn't do much because your fork only has the same stuff
as the main repo right now.  But at least executing this command and
getting no errors shows you have no mistakes.

## Working with branches

The best way to work in the fork-and-pull-request mode is for you to
do your work on branches.  Branches in git are very simple to create
and switch between, but are substantially different in nature from
branches in older version control systems like CVS.  Please see a good
reference about git (such as [Pro
Git](https://git-scm.com/book/en/v2/Git-Branching-Branches-in-a-Nutshell)
and [Think like (a) Git](https://think-like-a-git.net/)) to get a good
understanding of how they work.

Assume you're getting ready to start some work on a new feature or bug
fix.  Update your master branch and create a new branch from it:

```
cd /path/to/Xastir
git pull            # updates the code to the latest
git checkout -b my_feature_branch   # Creates a new branch from the
                                  # latest code on master
```
That last command creates a new branch and then switches to it.  Any
commits you make will go on the branch instead of on the master
branch.

## Doing your work

### verify you're on the right branch
Commit your work to your LOCAL repo and your own branch.  If you
followed the steps in the previous heading you've already created that
branch and switched to it.  But just to be sure, run
```
git status
```
which should show you:
```
> git status
On branch my_feature_branch

nothing to commit, working tree clean
```

If it doesn't say you're on the branch you expected you would be on,
switch to it with `git checkout my_feature_branch`.

### Do your work
Now do your work, which will change some files.

At any time, you can run `git status` and git should show you which
files you've changed, any new files you've created that are not
currently tracked in git, and so forth.  You should always do a "git
status" before taking the next step.

### Commit your work
When you're ready to save your work, either because it's finished or
because you've reached a good checkpoint and want to preserve it, add
the files you've changed to the staging area and commit them:
```
  git add <file1>  <file2> <file3> ...     # Adds all changed files
  git commit                               # actually saves the
                                           # changes in git
```

### Repeat "do your work" and "commit your work" until done

Typically, development will take a while and you'll have more than one
commit to do to get it all done.  It is best to make your commits
"atomic" by doing small changes, testing them like crazy, and
committing them rather than to make all of your work show up in one
huge commit touching many files all at once.

All of these small changes get committed to the
clone on your machine, without updating github at all.

## Publish your work

When your feature branch is done, push it *to your fork*.  Assuming
your fork has been added as a remote named "myremote" and your branch
is named `my_feature_branch`, that is done with:

```
   git push  myremote my_feature_branch
```

This will create a new copy of your feature branch on your own fork on
github.   The main repository will not be touched.

### Open a pull request

Go back to github to the main Xastir repository.  You should see a
banner near the top saying that you have added code to a branch on a
fork, and it should have a button that says "Compare and make pull
request" or some such words.

Open a pull request using this button.  The pull request is basically
an announcement to the development team that there is a suggested
change being made and provides a way for them to review those changes
and accept them, or to make comments on the changes and request
additional changes.

### Update your pull request

If your pull request gets comments saying that more change is needed,
you can continue to work on your branch, make more commits, and push
them to your fork.  Those commits will automatically be added to the
pull request for the reviewers to see.

### Work In Progres (WIP) pull requests

You can push your code to your fork and open a pull request at any
time, even if your work isn't finished.  The main reason to do this is
if you think your work is going to be very involved and touching lots
of code.  If you push incomplete work to your fork and open a pull
request for it before it's done, please put the text "WIP:" in the
pull request's name, and select the "Convert to draft" option
immediately after opening it.

This will allow reviewers to see what you're doing as you do it,
possibly relieving the burden of reviewing a single enormous set of
changes all at once.  By marking the pull request as a draft, nobody
is allowed to merge it until you change it to "Ready for review."

## Once your pull request is merged
When your pull request is accepted and merged into the master branch,
make sure you update your local clone.

Switch back to the master branch and pull it.

```
git status             # should show you still on your feature branch,
                       # and that the working directory is clean
git checkout master    # Switch back to the master branch, which is
                       # now outdated
git pull               # get all the changes that have been made to
                       # master, including your accepted PR
```

# Git Commit Message Format

> [!IMPORTANT]
> The Xastir team has a specific format we want you to use in commit
> messages.  Please read this section and follow it.

Git commit messages need to be in a certain format to make the best use
of the "git log" commands. In particular the first line needs to be 50
chars or less, then a BLANK LINE, then a detailed commit message. See
this link for more info:

http://chris.beams.io/posts/git-commit/

# Assorted git details

## Checking Out A Branch:

All branches associated with the Xastir project are contained in the clone
you made earlier. You can switch your current working directory to
one of those branches easily:

```
cd Xastir
git fetch (this updates your local repo copy from github, but doesn't
           merge changes into your working tree)
git checkout <branch name>    (this switches all the files in your working
                               tree to match those in the branch)
git merge                     (This makes sure that all the changes that
                               may have happened upstream on that branch
                               get into your copy)
```

You do not have to do this in a new directory --- so long as
you haven't changed any files in the source tree, git checkout
automatically swaps out all files it knows about with versions from the
branch.

## Keeping multiple working directories for multiple branches
If you really want to keep more than one branch's code around to work on,
you can do that if you have git version 2.5 or later with the following
commands:

```
cd Xastir
git worktree add <path> <branchname>
```

This will create a new directory tree called <path> with the named
branch checked out into it.


## There's a lot more.

There are many more git commands and options. Many of them are more of
use to the developers. Some of those are listed below. The above should
be enough for most people to keep their copies in sync with the latest git
development sources.


## If Using Multiple GitHub Accounts:

You may have trouble getting your commits attributed to the correct GitHub
login. GitHub uses the username/email in your git config settings for
attribution. If it is wrong, you may have to do some of the below in order
to set a LOCAL username and email for the one repository.

The user.name and user.email are pulled from the global git config, but a
local git config inside each repo will override those settings.

Go to root of checked-out repo and set local user.name and user.email for
this repo:
```
    git config user.name <github username>
    git config user.email <email address>
    git config -l           # Shows both local and global settings, hard to tell which is which
    git config --global     # Shows global settings
    git config --local -l   # Shows local repo configs, so should show new settings
```
Another method (but more error-prone) of editing local/global git config is:
```
   git config edit             # Edit local config
   git config --global edit    # Edit global config
```
If new commits still aren't using the right email, make sure you have not
set GIT_COMMITTER_EMAIL or GIT_AUTHOR_EMAIL environment variables. 


# For More Info:

Make sure you know how git works. Read https://git-scm.com/book/en/v2

If you are very familiar with CVS, get used to working differently, because
git is different.

Read and understand http://chris.beams.io/posts/git-commit/

Read http://justinhileman.info/article/changing-history/

Read http://think-like-a-git.net/

Read "Visual Git Cheat Sheet" at http://ndpsoftware.com/git-cheatsheet.html

Branching and merging in git is very simple, and is documented very well
by all those links. We will not repeat it here.

If you use SSH, set up your SSH keys on GitHub and do the "git clone" using
the SSH path. This will save you having to put in your password each time
you use the remote repository, although if you added a passphrase to your
SSH key you'll have to enter that each time.

# Useful Git Commands:

Set up global user/email
```
   git config --global user.name "Your Name"
   git config --global user.email "user@domain.com"
```

Set up user/email for a local repository
```
   cd /path/repository
   git config user.name "Your Name"
   git config user.email "user@domain.com"
```

Configure Git's editor:
```
   git config --global core.editor /path/to/editor
```

Colorizing Git output (set once and forget):
```
   git config --global color.ui auto
```

Clone a repo:
```
    git clone https://github.com/Xastir/Xastir
    git clone git@github.com:Xastir/Xastir
```

Status of local repo:
```
   git status
```

Diff for a file:
```
   git diff <filename>
```

See all branches, local and remote:
```
    git branch -a
```

Visual Git viewer:
```
    gitk (tcl/tk and generic X11 viewer, comes with git)
    or
    gitg (gnome git viewer)
```

Add files to the staging area:
```
    git add <file1> <file2>
```

Commit changes to LOCAL repo:
```
    git commit      # If have files in staging area already
    git commit <file1> <file2>  # Ignores staging area
```

Push local changes to remote repository:
```
    git push
```

Update local repo from remote repo
```
    git fetch
```

Update local repo and merge remote/local changes in local repo (May be
dangerous for developers with modified code in their working tree who
are unwisely working directly in the master branch):
```
    git pull
```

Rebase local changes against latest master branch (can be a safer way
of updating master if you've unwisely been changing things in that
branch locally)
```
    git fetch
    git rebase master
```

------------------------------------------------------------------------
Copyright (C) 2000-2026 The Xastir Group


