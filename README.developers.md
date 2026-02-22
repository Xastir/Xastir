# Developer notes on creating releases

Since the move to git, the old process we followed to push development
snapshots and stable releases to SourceForge is no longer possible,
nor especially helpful.  With git and github, we are doing away with
the concept of development snapshots, because one can always just
download a tarball of the current repo state from github.  We are also
simplifying the stable release process, doing away with the difference
between building a tarball release and building from a git clone.


## Development snapshots

Xastir migrated to git and github instead of cvs and sourceforge, and
therefore creating "development snapshots" isn't necessary, because
every commit is essentially a development snapshot that can be checked
out by referencing its SHA-1 hash.  Furthermore, github allows users
to download code as a tarball directly without needing to clone, so we
need not make a duplicate process for it.

## Stable Releases

Stable releases are the enduring, numbered releases that tend to make
it into official package repositories (eventually).  It is necessary
to do more for these releases than just tag a repository state,
because most package management systems require that a stable version
of the code be downloadable, and don't support pulling
versions-of-the-day out of source code management systems.

Beginning with release 2.1.8 we stopped providing "configure" scripts
and all the droppings from "bootstrap.sh" in release tarballs, and all
users must now use "bootstrap.sh" as a first step in building Xastir.

### Stable release process in a nutshell

- Get master ready for a release.
- Update version number.
- Test everything.
- Tag the repo.
- Push the repo and the tag to Github.
- Define a release on github and associate it with the tag.
- Email interested parties that there has been a release.
- Go back and update the version number on master and move on.

### Stable release in gory detail

- Make sure the current state of the master branch is what you want to
  release.  This should include all documentation updates and help
  file updates.  Only when the master branch is really ready to
  release do you perform the following steps.  Let's assume we're
  creating release X.Y.Z, and that our Xastir clone and working
  directory is in ~/XASTIR/Xastir.

- By our long-standing convention, stable releases are always even
  numbers in the last field of the release number, and odd numbers
  mean "this is a development version."  So whatever version number
  appears in configure.ac on the master branch is going to be odd at
  the moment, and you're going to pick X.Y.Z so that the new Z is
  even.

- Change the version number in configure.ac to X.Y.Z.  There should
  no longer be any other places in the code where this version number
  appears, so changing it in this one place is sufficient.  Commit
  this change:

      git add configure.ac
      git commit

  Mention why you're doing this in the commit message (e.g., "Update
  release version number").  Follow our commit log message guidance in
  CONTRIBUTING.md

- Run bootstrap.sh or "autoreconf -i"

- Make sure the program builds, and do so in a fresh build directory,
  just as a first-time user would have to do:

      mkdir build-release-check
      cd build-release-check
      ../configure [options]
      make
      cd ..

  If the code builds you should be in good shape, and you should also
  try querying the binary it produced to have it print its version:

      build-release-check/src/xastir -V

  Confirm that it is reporting the version you expect it to.  It will
  have additional decorations indicating stuff about git, ignore those.

  For safety's sake, you should remove the build directory now, too.

      rm -rf build-release-check

- You now have a working directory that should look like what we want
  to distribute to users.  Check that there are no uncommitted
  changes:

      git status

  should tell you you're on master, and that you're one commit ahead
  of "origin/master", with nothing to commit and a clean working tree.
  If it says anything else, figure out why and get the current working
  tree to the right state, with all important changes committed
  properly.

- Create an annotated tag marking the current state of the repo as
  your new release:

      git tag -a -m "Xastir Release X.Y.Z" Release-X.Y.Z

  Don't forget the "-a", as this is what allows our use of "git
  describe" to give the user an accurate description of what code
  they're running if they're running a development version later.

- At this point, you are almost done, but all of your changes are only
  in your local repository clone.  Double check that it really works
  by creating a tar file of your code from the tagged state, then try
  to build it somewhere other than in your git checkout directory:


      git archive --format=tar.gz --prefix=Xastir-Release-X.Y.Z/ Release-X.Y.Z > ~/src/Xastir-Release-X.Y.Z.tar.gz

  This process will exactly reproduce what Github will be doing when
  we're finished and actually create the release.  Now make sure it builds:

      cd ~/src
      tar xzf Xastir-Release-X.Y.Z.tar.gz
      cd Xastir-Release-X.Y.Z
      ./bootstrap.sh          # You could also use "autoreconf -i"
      mkdir build
      cd build
      ../configure [options]
      make

  This is strictly a sanity check to make sure that everything that's
  in the repository really, truly is ready to go.  If you've been
  following good coding and testing practices and the continuous
  integration testing is working properly, this step isn't really
  necessary.  But it's good to be sure.

- If the sanity check above worked, you can throw away the testing
  tarball and unpacked code:

      cd ~/src
      rm -rf Xastir-Release-X.Y.Z Xastir-Release-X.Y.Z.tar.gz

  - If the sanity check did NOT work, then you need to go back to
    your original working directory and fix any problems you found.
    Commit your changes, and then MOVE THE TAG so it points to your
    NEW proposed release:

      git tag -d Release-X.Y.Z
      git tag -a -m "Xastir Release X.Y.Z" Release-X.Y.Z

    Now go back and redo the sanity check.  Repeat until the tarball
    you created actually produces a working Xastir.

- Now go back to your working directory and finish up by pushing the
  code and tag to Github:

      cd ~/XASTIR/Xastir
      git push origin master
      git push origin Release-X.Y.Z

- Log in to github and go to the Xastir project releases page at
  http://github.com/Xastir/Xastir/releases.  Click the "Draft a new
  release" button.  Put your tag name (Release-X.Y.Z) into the
  dialog box that says "Tag version" and Github will display a note
  that it found a matching, existing tag.  Fill in the rest of the
  form:

    - Give the release a name ("Xastir Release X.Y.Z") that will
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

    https://github.com/Xastir/Xastir/archive/Release-X.Y.Z/Xastir-Release-X.Y.Z.tar.gz

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


#### Getting master ready to move on

All of this work got the X.Y.Z release done, which has now been
finished and pushed to github.  Now we need to change the version
number on the master branch so that development versions show a
different version than releases.


- Make sure you're still in your master branch in your main clone:

      cd ~/src/Xastir
      git checkout master


- Edit configure.ac and change the version number to be one higher than the
  release you just did.  So if you just pushed release 2.1.8, set the
  version to 2.1.9.

- Commit this change and push it to github.

      git add configure.ac
      git commit
      git push

  The release is done, and now the repo is ready for further development.
