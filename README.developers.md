# Developer notes on creating releases

Since the move to git, the old process we followed to push development
snapshots and stable releases to SourceForge is no longer possible,
nor especially helpful.  With git and github, both types of releases
are simpler.

## Development snapshots

Xastir migrated to git and github instead of cvs and sourceforge, and
therefore creating "development snapshots" isn't necessary, because
every commit is essentially a development snapshot that can be checked
out by referencing its SHA-1 hash.  However, if one wants to make it
easy to grab a specific snapshot via a tar or zip file, one merely
needs to create a tag and push it to github.  It will then be
available as a tar.gz or zip download without using git.

- First decide if this is really needed.

  The only reason to make a development snapshot is to create a known
  point in the revision history that can be downloaded very easily
  without using git at all.  Any point in the revision history may
  always be accessed using git by that state's "SHA-1" hash to "git
  checkout":

      git checkout 9a20aca

  will cause the files in the current directory to be swapped out with
  the versions that were current when commit 9a20aca was made.  One
  can get back to the current master branch with

      git checkout master

  But if you have a good reason for declaring today's code worthy of a
  development snapshot, you do this by making a tag.

- First create an "annotated tag" for the snapshot

      git tag -a -m "Xastir development snapshot" dev-snapshot-DD-MM-YY

  In the past, we used odd numbered versions for the development
  snapshot releases, and even for stable releases, but by using a tag
  similar to the one above it is more clear that this is just a
  snapshot of the development branch of code, and when it was created.

- Push the tag to github

      git push origin dev-snapshot-DD-MM-YY

  The new tag will appear on the "Releases" page in the "Tags" tab:
  https://github.com/Xastir/Xastir/tags where one can click on a link
  to get a tar or zip file.  It may also be downloaded by the direct
  link https://github.com/Xastir/Xastir/releases/tag/tagname where
  tagname is whatever you used for the tag you just created.

## Stable Releases

Stable releases are the enduring, numbered releases that tend to make
it into official package repositories (eventually).  It is necessary
to do more for these releases than just tag a repository state,
because most package management systems require that a stable version
of the code be downloadable, and don't support pulling
versions-of-the-day out of source code management systems.

It is also necessary that the tarballs associated with a release be
complete and ready to build, which means they must contain the files
that "bootstrap.sh" creates and not require the user to run this
script (and to have automake and autoconf installed).

### Stable release process in a nutshell

- Get master ready for a release.
- Create a release branch and switch to it.
- Update version number.
- Make git stop ignoring bootstrap artifacts on the branch.
- Bootstrap.
- Commit bootstrap artifacts to the branch.
- Test everything.
- Tag the branch.
- Push the branch and the tag to Github.
- Define a release on github and associate it with the tag.
- Email interested parties that there has been a release.
- Go back and update the version number on master and move on.

### Stable release in gory detail

- Make sure the current state of the master branch is what you want to
  release.  This should include all documentation updates and help
  file updates.  Only when the master branch is really ready to
  release do you perform the following steps.

- First, create a release branch and check it out.  This is done most
  conveniently using git "worktrees" (a feature that requires a
  version of git at least as new as 2.5).  So for release 2.1.0, we
  would do:

      cd ~/src/Xastir # assume this is our main git clone
      git worktree add -b branch-release-2.1.0 ~/src/Xastir-2.1.0

  This command will create a new branch "branch-release-2.1.0" and
  then create a new directory ~/src/Xastir-2.1.0 with that branch
  checked out.  The current working directory will still contain the
  master branch.

- Do the remaining steps in the new worktree directory:

      cd ~/src/Xastir-2.1.0

- Change the version number in configure.ac to 2.1.0.  Grep around the
  code and remember to fix any other places where the old version
  string appears.  Commit this change:

      git add configure.ac
      git commit

  Mention why you're doing this in the commit message (e.g., "Update
  release version number").  Follow our commit log message guidance in
  CONTRIBUTING.md

- Edit .gitignore and comment out the lines of file names that relate
  to the files that bootstrap.sh creates.  These are all at the top of
  the file and the comments should tell you where to start commenting
  out and where to stop.

  Go ahead and commit *this* change, too:

      git add .gitignore
      git commit

  Mention why you're doing this in the commit message (e.g., "Disable
  ignoring of bootstrap-produced files for release") .  Follow our
  commit log message guidance in CONTRIBUTING.md

- Run bootstrap.sh

- Make sure the program builds:

      mkdir build
      cd build
      ../configure [options]
      make
      cd ..

  For safety's sake, you should remove the build directory now, too.

      rm -rf build

- You now have a working directory ~/src/Xastir-2.1.0 that should look
  like what we want to distribute to users, and the files created by
  bootstrap should no longer be ignored.  Make sure:

      git status

  should show:

      Makefile.in
      aclocal.m4
      autom4te.cache/
      callpass/Makefile.in
      compile
      config.guess
      config.h.in
      config.sub
      config/Makefile.in
      config/language-ElmerFudd.sys
      config/language-MuppetsChef.sys
      config/language-OldeEnglish.sys
      config/language-PigLatin.sys
      config/language-PirateEnglish.sys
      configure
      depcomp
      help/Makefile.in
      install-sh
      m4/Makefile.in
      missing
      scripts/Makefile.in
      src/Makefile.in
      src/rtree/Makefile.in
      src/shapelib/Makefile.in
      src/shapelib/contrib/Makefile.in
      symbols/Makefile.in

  This is just what we want, so commit it:

      git add .
      git commit

  Mention why you're doing this in the commit message (e.g., "Commit
  bootstraped files for release").  Follow our commit log message
  guidance in CONTRIBUTING.md


- At this point, you are almost done, but all of your changes are only
  in your local repository clone.  Double check that it really works
  by creating a tar file of your branch, then try to build it
  somewhere other than in your git checkout directory:


      git archive --format=tar.gz --prefix=Xastir-Release-2.1.0/ branch-release-2.1.0 > ~/src/Xastir-Release-2.1.0.tar.gz

  This process will exactly reproduce what Github will be doing when
  we're finished and actually create the release.  Now make sure it builds:

      cd ~/src
      tar xzf Xastir-Release-2.1.0.tar.gz
      cd Xastir-Release-2.1.0
      mkdir build
      cd build
      ../configure [options]
      make


- If the sanity check above worked, you can throw away the testing
  tarball and unpacked code:

      cd ~/src
      rm -rf Xastir-Release-2.1.0 Xastir-Release-2.1.0.tar.gz

- Now go back to your worktree and finish up by creating a new tag for
  the release and pushing the release branch and tag to Github:

      cd ~/src/Xastir-2.1.0
      git push -u origin branch-release-2.1.0
      git tag -a -m "Xastir Release 2.1.0" Release-2.1.0
      git push origin Release-2.1.0

- Log in to github and go to the Xastir project releases page at
  http://github.com/Xastir/Xastir/releases.  Click the "Draft a new
  release" button.  Put your tag name (Release-2.1.0) into the
  dialog box that says "Tag version" and Github will display a note
  that it found a matching, existing tag.  Fill in the rest of the
  form:

    - Give the release a name ("Xastir Release 2.1.0") that will
      appear prominently above it in the releases list.

    - Enter some release notes in the large text box below the title
      where it says "Describe this release."  Ideally, you should list
      release highlights (new features, bug fixes, etc.).  Use
      Markdown to pretty up the text, using the Preview tab to render
      the markdown until it looks the way you want it to.

    - Click "Publish Release."


- You have finished releasing the code as far as Github is concerned.
  This new release will now appear on the "Releases" page, along with
  links to tar and zip files for the source code and the release notes
  you just created.  The fixed URL
  https://github.com/Xastir/Xastir/releases/latest will always point
  to the most recent release.  The source code download link will be

    https://github.com/Xastir/Xastir/archive/Release-2.1.0/Xastir-Release-2.1.0.tar.gz

  with the obvious change for the zip version.

- The last step here is to announce the new release in all the usual
  places.  These days it is probably enough to announce it on the
  xastir mailing list, and possibly the aprssig and linux-hams groups.
  No need to spam every ham radio mailing list.  On the other hand,
  the Xastir wiki does recommend sending notification of all releases
  (both development and stable) to:

      - xastir at xastir.org
      - nwaprssig at nwaprs.info
      - aprssig at  tapr.org
      - aprsnews at tapr.org
      - macaprs at yahoogroups.com
      - aprs at yahoogroups.com

  and stable releases to:

      - SAR_APRS at yahoogroups.com
      - CSAR at yahoogroups.com
      - aprs at mailman.qth.net
      - linux-hams at vger.kernel.org
      - linux at tapr.org
      - linux-hams-using-ax25 at yahoogroups.com

  This list is probably excessive nowadays, and probably contains a
  lot of groups that are long gone.

- You may remove your worktree for Xastir-2.1.0 now, because all of your
  work is saved in your own git repo and on github:

      cd ~/src/Xastir
      rm -rf ~/src/Xastir-2.1.0
      git worktree prune      # not really necessary, as git will do this itself
                              # eventually


#### NEVER MERGE RELEASE BRANCHES BACK TO MASTER

The steps we took above are almost all done this way because in general we
ignore all the files bootstrap.sh produces and never commit them to the
master branch of the repository.

Github releases are not just static copies of release tarballs we
upload the way they were with SourceForge --- they're taken directly
out of the repo using something very similar to the "git archive"
command we used testing out our release.  It was therefore necessary
to change .gitignore so it doesn't ignore those files, then commit
them.

Because of this, we never want to do a release branch to master merge.
Just don't do it.  Leave the release branch unmerged forever.

#### Getting master ready to move on

All of this work was on your release branch, which has now been
finished and pushed to github.  Now we need to change the version
number on the master branch so that development versions show a
different version than releases.


- Make sure you're still in your master branch in your main clone:

      cd ~/src/Xastir
      git checkout master


- Edit configure.ac and change the version number to be one higher than the
  release you just did.  In the example we've been working, that would be 2.1.1.

- Commit this change and push it to github.

      git add configure.ac
      git commit
      git push
