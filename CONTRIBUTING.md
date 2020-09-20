# Contributing to the Xastir project

Hello possible new developer, and welcome to the Xastir project!

## Ways to contribute

There are many ways to contribute to a project, and not all mean that
you have to write code.

* You can ask and answer questions on the Xastir mailing list
* You can report bugs
* You can fix bugs
* You can add or modify features in Xastir

### Asking and answering questions

Please subscribe to our mailing list as listed on the official Xastir
web site, http://xastir.org/.  We would prefer it if you would not use
Xastir's git hub issue tracker just to ask questions, as this is not
the ideal forum and doesn't reach the full Xastir user community.
Your best bet is to get your question in front of as many users' eyes
as possible, and the mailing list is the way to do that.

Once you're confident enough with Xastir to be answering users'
questions, your presence on the mailing list will naturally allow you
to see and answer those questions, too.

### Reporting bugs

If you believe you might have found a bug, your first step might be to
ask about it on the mailing list to see if it's really a bug, if
you're making a mistake, or if it's just a difference between what you
expect and what the code actually does.

If you are reasonably sure that you have found a real bug and wish to
report it, open an issue on the Xastir Github repository at
https://github.com/Xastir/Xastir.

Realize that Xastir is entirely a volunteer effort largely moved
forward by very busy people, and your bug might not get fixed in short
order, and it might not even get fixed unless you fix it yourself.

### Fixing bugs

Another way you can contribute is to fix bugs yourself and submit pull
requests to the group for your changes to be added to the main code
base.  You could fix bugs that you find yourself, or could look
through the existing list of issues on Github and pick one to fix.

If you do this, you will want to follow the procedure below that
describes how to submit code changes for review and acceptance.

### Adding or modifying features in Xastir

Maybe Xastir is lacking some feature you really want, or you really
would like to make some other feature of Xastir work differently.  The
first step here might be to start a discussion on the mailing list to
see if other users like your addition or modification before pressing
ahead with it, but you could always just go ahead with the development
and offer it as a suggested change through the pull request mechanism.
Asking on the mailing list first is more likely to get a faster
response when your pull request shows up..

## General guidelines for contributing code

In order to get contributed patches accepted more easily by the
Xastir developers:

* Read "Developer Guidelines and Notes" in the "HowTos" section of
Xastir's Wiki (http://xastir.org).  Make sure to follow the formatting
and indentation rules, and in particular the tab format (spaces, not
tabs).  If you don't like some of the formatting rules, abide by them
anyway for consistency.  Some of the developers don't like some of the
formatting rules either, but consistency is more important than ideas
we might have of coding style!

* Check the Xastir issue tracker: http://github.com/Xastir/Xastir/issues
You need a GitHub account to create new issues, but this is free.
This is one of the best places to see what needs to be worked on,
and to see if anyone else has had a similar idea. 

* Check with the Xastir-Dev list first to see if anyone else is
working on that particular idea or section of code.

* Verify on the Xastir-Dev list that your idea has some merit and
chance of being accepted before you put your valuable time into the
patch.

* Make sure you're willing to abide by the GPL license with respect
to your patch.

* Use as generic C as possible, and comment what you've done, in
English please!

* Keep in mind that Xastir runs on multiple operating systems, so
code for that.  Some #ifdef's may be required in order to make it
work for the various operating systems.

* Xastir can be run in multiple languages, so code for that.  If
any user text is added, make sure to add language strings for them
to the language files.  If you don't know a particular language, add
it to all of the language files in English.  It will be translated
by others later.

## Contributing code changes to the project

While it is always possible to create a patch set and email the
result to Xastir developers, this is no longer a desirable way to
proceed.  The preferred method of contributing source changes is
through GitHub "pull requests."  (See
https://help.github.com/articles/about-pull-requests/)


In a (rather large) nutshell this process goes like this:

* Log in to Xastir's github repo at
  https://github.com/Xastir/Xastir.git and click the "Fork" button.
  This will create a copy of the repo that you have full control of.
  Once you have created a fork, here's a general approach you can use:

* Clone your repo:

      cd <some place other than where your other Xastir clone lives>
      git clone https://github.com/youruserid/Xastir.git
      cd Xastir

  The result of this step is that you will have a primary remote
  named "origin" that points at your forked repository.  It will also have
  checked out the default branch, which in the case of the Xastir repo
  is just the "master" branch.

* Add the official repo as a second remote called "upstream":

      git remote add upstream https://github.com/Xastir/Xastir.git

  Now your local repo knows about two remote repos -- yours, and the official
  project repo.  Now make git sync to this remote, too:

      git fetch upstream

* Create a topic branch to work on:

      git checkout -b <branchname>

* Do all your work while checked out in this branch.  As you work,
  use git add/git commit commands to save your changes in logical
  chunks.  Staging your changes in multiple commits this way helps
  keep the project history readable.

  See the notes below regarding formatting of commit messages, please.

* When you have finished your work (or when you think you have) and are
  ready to ask for it to be reviewed and accepted, you will need to
  publish your changes to GitHub and initiate a pull request.
  * First, make sure your repo has an up-to-date master branch:

    ```
    git fetch upstream            # This fetches changes in the official rep
    git checkout master           # go back to the master branch
    git merge upstream/master     # brings your local master up to date
    git push origin                 # this brings your GitHub fork up to date
    git checkout <branchname>       # get your branch back
    ```

  * Always rebase your feature branch so that it branches off of the
    current head of the master branch:

    ```
    git rebase master               # makes sure that your branch is based
                                    # on the most recent version of master
    ```
    
  * Now publish your branch in your GitHub fork (your "origin" remote).

        git push -u origin <branchname>

    Now your changes exist on the named branch in your GitHub repo, and may
    be shared with others.

    NOTE: If you have refrained from touching the code on the master
    branch, the merge of upstream master will always be as simple as
    shown here, with no conflicts possible --- all you're doing is
    grabbing what others have done while you were working.  In times
    of great activity on master (there are seldom such times) it is
    possible, though, that the rebase operation can show conflicts
    between upstream changes and your changes that you will have to
    fix yourself.  Resolving these conflicts requires editing your
    code, fixing the conflict markers, then doing additional git
    add/git commit operations and a third command to tell git rebase
    to continue after the conflict resolution.  Please see the Pro Git
    book at https://git-scm.com/book/en/v2 for guidance should this
    happen to you.

  * A good practice at this point, before you've asked the Xastir developers
    to merge your work, is to send an email to the xastir-dev mailing list
    and ask others to test out your work.  They can easily check out your
    fork and make sure there are no hidden issues you hadn't detected.
    See below for a suggestion of the easiest way to check out other people's
    topic branches.

  * Now you are ready to ask for your code to be reviewed and accepted.

    * Open your web browser to your GitHub repo for your fork at
      https://github.com/youruserid/Xastir.git.

    * Using the pull-down menu, select your feature branch name.

    * To the right, you'll see a button marked "Pull request."  Click it
      to begin the process of creating an automated request to pull your
      code into the main repo.  Fill out all of the form and create
      a pull request.

    * A member of the Xastir project will review your changes and
      comment on the pull request.  If the changes are
      straightforward, your request may be accepted directly, or you
      may be requested to make further changes.

    * If you need to add more commits to address concerns brought up in the
      review of your pull request, just make them on your topic branch and
      push them again:

         ```
         git checkout <topic branch>
         <edit your files>
         git add <files>
         git commit
         git push
         ```

      Since you have already associated your topic branch with your
      forked remote in a previous step, it is no longer necessary to
      say "-u origin branchname" here.

    * If this goes on for very long, it could come to pass that master has
      changed again.  If so, you may need to rebase and force-push your
      topic branch.  Ideally, you should endeavor to make your pull requests
      as easy to test and review as possible, so that it doesn't take forever
      for developers to get around to testing them and merging them.

  There are lots of guidelines out there about how to make good pull
  requests.  Please read
  https://github.com/blog/1943-how-to-write-the-perfect-pull-request
  for one such article.

  There is also a pretty nice tutorial on how to do your first pull request
  at https://www.thinkful.com/learn/github-pull-request-tutorial/

## Git Commit Message Format
Git commit messages need to be in a certain format to make the best use
of the "git log" commands. In particular the first line needs to be 50
chars or less, then a BLANK LINE, then a detailed commit message. See
this link for more info: http://chris.beams.io/posts/git-commit/

## Checking out other people's work

It was mentioned above that sometimes it is a good practice to ask other
people to check out your code changes before a pull request is opened.
This section contains some simple ways for Xastir users and developers
to check out code in other people's repos with a minimum of fuss.

Say user "sumgai" has a topic branch named "sumnufeetyur" in a fork of Xastir
at "https://github.com/sumgai/Xastir.git".  You want to pull those changes
and test them out.  This can always be done just by cloning the entire fork,
but that is not necessary, since most of the fork is a straight copy of the
official repo.   You can instead just grab sumgai's changes:

* I'm assuming you're already checked out from the master branch, but
  let's make sure:
 
      git checkout master

* In your regular Xastir clone directory, create a new branch to hold
  sumgai's work, and switch to it:

      git checkout -b sumgai-sumnufeetyur

  When you do this, all you've done so far is to create a new name for
  a copy of master.

* Now pull sumgai's branch into yours:

      git pull https://github.com/sumgai/Xastir.git sumnufeetuyr

  Your sumgai-sumnufeetyur branch will now match sumgai's sumnufeetur branch
  exactly.  This does generally work best if sumgai has kept his branch
  rebased off of master, which is why we recommend that approach.

* You can now build the code and test it as usual.

* When you're done testing sumgai's code, you can go back to the
  unmodified Xastir just by doing "git checkout master," and
  rebuilding the code.  If you no longer want to keep a copy of
  sumgai's work around, you can delete the branch with
  
      git branch -D sumgai-sumnufeetyur

**Note to developers**:  The process of doing this checkout of others' branches
is exactly what GitHub recommends as the first steps to resolving pull
requests manually through the CLI instead of through github's web interface.


## Debugging hints

Xastir is a multi-threaded and multi-process application.  It uses
both threads and forks to do it's work.  You must have a debugger
that is capable of following these kinds of twists and turns in a
program.  Many older versions cannot.

### Old notes on using GDB with Xastir

*The notes in this section were taken from an old email thread and were written by Tom Russo*

According to everything I can find about GDB, debugging of
multithreaded apps has been supported in gdb for some time, and are
certainly supported in gdb 6.x.  For the last couple days I've been
running xastir inside gdb instead of at the command prompt ---
perhaps I won't keep forgetting to start it with -t now, and when it
crashes I'll be where I need to be.

This is possibly an option for you, too, if you're seeing
segfaults frequently enough and can't solve the core file question.
Try this:

    gdb /usr/local/bin/xastir
    > run -t

*The ">" above is a gdb prompt --- don't type it!*

When it crashes, it'll pop you right out into the debugger, whether
a core file was created or not.  You could then view the active
threads:

    info threads

and get a stack trace of where the crash happened:

    where

You could also probably list the code near where the crash happened:

    list

If you send the output of those three commands to the xastir-dev
list then it might help us narrow down the causes.

### Other old debugging notes

*This section was also taken from an old email chain, and was written by Curt Mills*

Note that the meaning of the "-t" command-line flag has been
reversed *[Ed. Note: from what it was in earlier days of Xastir]*.
"xastir -?" should show the change once you compile it.
This means that we'll do core dumps by default upon segfault instead
of using the internal Xastir segfault handler.  We've also
re-enabled the "-g" compile option for GCC so that debugging
information will remain embedded in the executable (unless you strip
it).  This should aid the development team to debug problems.

> Also, I cannot find anything in man bash that talks about core
> dumps nor segmentation faults.

Seems like some stuff we're just supposed to know.  _How_ we're
supposed to know I don't know either...  ;-)

Perhaps that last bit about SUID/SGID might be a clue:  There are
often exceptions to the rules for SUID/SGID programs.  Try _not_
setting Xastir SUID (if you're not using AX.25 kernel networking
ports that is) to see if you get a core file.  Can you send a
SIGSEGV to the running process and make it dump?  I just tried this
and I'm not getting a core file either.

Ah, I see in the "man bash" page:

    ulimit
        -a  All current limits are reported

This gives me:

    core file size        (blocks, -c) 0
    data seg size         (kbytes, -d) unlimited
    file size             (blocks, -f) unlimited
    max locked memory     (kbytes, -l) 32
    max memory size       (kbytes, -m) unlimited
    open files                    (-n) 1024
    pipe size          (512 bytes, -p) 8
    stack size            (kbytes, -s) unlimited
    cpu time             (seconds, -t) unlimited
    max user processes            (-u) 6139
    virtual memory        (kbytes, -v) unlimited

So...  Looks like I need to set "ulimit -c" and try again:

    ulimit -c unlimited

Now when I do "kill -11 <pid>" I get this:

    [1]+  Segmentation fault      (core dumped) xastir

and this:

    -rw-------   1 archer users 12320768 2006-02-02 09:31 core.7586

I just added "ulimit -c unlimited" to my .profile.  For what it's
worth, if Xastir has SUID permission bit set (4755) I don't get a
core dump, but if it is reset I do (0755).

I can invoke ddd like this (it uses gdb under the hood):

    ddd /usr/local/bin/xastir core.7586 &

or gdb like this:

    gdb -c core.7586 /usr/local/bin/xastir

Once you have the program and core file loaded into the debugger you
can display a backtrace to see where the program died.  In the case
of ddd, it's Status->Backtrace, then you click on one of the entries
to make the source code window display the exact location.

Another good debugger to try is "UPS".

Another note about core files:  Sometimes they get written where you
don't expect.  I just had one appear in ~/.xastir/tmp, so if you
think you should have a core file and can't find it, try:

    find . -type f | grep core

The core file may be written to a directory that Xastir is currently
in, instead of where you started Xastir from.

### Customized debugging builds

Sometimes it is helpful to build xastir with specialized compiler
options to aid in debugging.  For example, if you're trying to hunt
down elusive segfaults, and you're using GCC, it might be wise to
build with "-O -g -fno-inline" to prevent the compiler from optimizing
quite so vigorously; aggressive optimization can sometimes lead to the
debugger saying the code died on one line when in fact it's happening
somewhere else.

To build with custom CFLAGS like this, just tell configure what you
want CFLAGS to be:

    mkdir -p build
    cd build
    ../configure CFLAGS="-O -g -fno-inline"

Naturally, unless you're using build directories separate from the
source directory (see below under "Segregating specialized builds"),
you need to build every file after changing configure like this, so do
a make clean before building:

    make clean
    make 
    make install

Remember not to "make install-strip" when trying to do debugging
builds, or your core files will not have debugging symbols in them and
it will be harder to get useful information out of the debugger.

### Segregating specialized builds

The automake/autoconf setup of xastir makes it easy to maintain several
different builds of the code without having to clean out the directory and
rebuild every time.  One does this with "build directories" and executable
suffixes.

To use this capability, make sure you're starting with a completely
clean source directory.  In the directory where you normally do your
"git pull", do a make distclean.  This will remove anything that
configure created as well as anything that make created.  From this
point forward, you don't build xastir in the source code directory
anymore.

Make an empty directory somewhere --- this will be your build directory.  
I put my build directories in parallel with the source code directory.  So
my tree looks like this:

    XASTIR-DEV
     |
     +----/Xastir (source directory)
     |
     +---/build-normal
     |
     +---/build-debug
     
and so on.  Some developers put their build directories inside the
Xastir tree instead.  It doesn't matter, but does change how you
invoke configure (you have to use the correct relative path to
configure below).

So I would do:

    cd XASTIR-DEV
    mkdir -p build-normal

In the build directory, you run configure using the path to the configure 
script:

    cd build-normal
    ../Xastir/configure
    make
    make install

The configure script uses the path that you gave when you ran it to figure 
out where to find the source code.

The beauty of this is you can make a second build without doing a make clean:

    cd XASTIR-DEV
    mkdir -p build-debug
    cd build-debug
    ../Xastir/configure --program-suffix="-debug" CFLAGS="-O -g -fno-inline"
    make && sudo  make install

This will build a second binary called "xastir-debug" and install it,
but because you've done the build in a separate directory, your normal
compile is untouched.

    cd ~/XASTIR-DEV/xastir
    git pull
    cd ../build-normal
    make && sudo make install
    cd ../build-debug
    make && sudo make install

will update both of your builds.

I also use this technique to share a single xastir source tree over
NFS, building the code for multiple operating systems in separate
build directories.

    (log in to CYGWIN machine, mount NFS directory)
    cd /mounted/directory/XASTIR-DEV
    mkdir -p build-cygnus
    ../Xastir/configure
    make && make install

It can also be used to maintain specialized configurations, for
example a build with "rtree" disabled, a build with map caching
disabled, etc.  This is a good development trick to keep yourself
honest --- make sure you can still build all your custom builds when
you've done a large hacking run, and want to check if you have broken
things inside ifdefs.  Doing that without build directories requires a
huge number of configure/make/make distclean cycles.

## More?
Anything else?  Let's hear about what's still confusing or needs to
be expanded in this document.  Thanks!


APRS(tm) is a Trademark of Bob Bruninga

Copyright (C) 2000-2019 The Xastir Group

