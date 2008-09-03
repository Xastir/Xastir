#!/usr/bin/perl -W

# $Id$

# Copyright (C) 2008  The Xastir Group
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License
# as published by the Free Software Foundation; either version 2
# of the License, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
# 02111-1307, USA.


# Look at the README for more information on the program.
#
# Run it like this:
#
#   cd xastir/config
#   ../scripts/langPirateEnglish.pl -split <language-English.sys >language-PirateEnglish.sys
# or
#   ../scripts/langPirateEnglish.pl <some-input-file >some-output-file
#
# "-split": Translate 2nd part of line only (Xastir language file).
# Without it:  Translate entire text.


# Regex strings derived from:
# http://userscripts.org/scripts/review/25998
# http://dougal.gunters.org/blog/2004/08/30/text-filter-suite


# Check whether we're translating an Xastir language file or plain
# text:
#   "-split" present:  Translate the 2nd piece of each line.
#   "-split" absent:   Translate the entire text.
my $a;
if ($#ARGV < 0) { $a = ""; }
else            { $a = shift; }
$do_split = 0;
if (length($a) > 0 && $a =~ m/-split/) {
  $do_split = 1;
}

while ( <> ) {

  # Change the "Id:" RCS tag to show that we translated the file.
  if (m/^#.*\$Id:/) {
      print "# language-PirateEnglish.sys, translated from language-English.sys\n";
      print "# Please do not edit this derived file.\n";
      next;
  }
  # Skip other comment lines
  if (m/^#/) {
    next;
  }

  if ($do_split) {
    # Split each incoming line by the '|' character
    @pieces = split /\|/;

    # Translate the second portion of each line only
    $_ = $pieces[1];
  }

  # Custom for Xastir:
  s/\bham\b/matey/g;
  s/\bhi hi\b/it be a joke, matey/g;
  s/\bk7gps\b/Th\' Good Cap\'n/g; 
  s/\bDave Dobbins\b/Th\' Good Cap\'n/g; 
  s/\bAPRS\'er\b/Fellow Pirate/g;  
  s/\b[Aa]prs\'er\b/fellow pirate/g; 
  s/\bHerb Gerhardt\b/scurvy dog/g;
  s/\bkb7uvc\b/scurvy dog/g;
  s/\bwa7nwp\b/cabin boy/g;
  s/\b[Mm]ap(s*)\b/Treasure Map/g;
  s/\bXastir\b/HMS Xastir/g;
  s/\bxastir\b/HMS xastir/g;
  s/\bXASTIR\b/HMS XASTIR/g;
  s/\bStation/Ship/g;
  s/\bstation/ship/g;
  s/\bView/Gander/g;
  s/\bview/gander/g;
  s/\bFile/Scroll/g;
  s/\bfile/scroll/g;
  #s/\bFile/Parchment/g;
  #s/\bfile/parchment/g;
  s/\bMessage/Dispatch/g;
  s/\bmessage/dispatch/g;
  s/\bLogging/Scribblin'/g;
  s/\blogging/scribblin'/g;
  s/\bLog/Scribble/g;
  s/\blog/scribble/g;
  s/\bPrint/Affix to parchment/g;
  s/\bprint/affix to parchment/g;
  s/\bCancel/Nay/g;
  s/\bcancel/nay/g;
  s/Close/Nay/g;
  s/close/nay/g;
  s/\bOK/Aye/g;
  s/\bOk/Aye/g;
  s/\bok/aye/g;
  s/\bQuit/Nay/g;
  s/\bquit/nay/g;
  s/\bExit/Run Away!/g;
  s/\bexit/run away!/g;
  s/\bSelect/Choose 'yer Weapon!/g;
  s/\bselect/choose 'yer weapon!/g;
  s/\bCompressed/Scrawny/g;
  s/\bcompressed/scrawny/g;
  s/\bAudio/Racket/g;
  s/\baudio/racket/g;
  s/\bFont/Scribble/g;
  s/\bfont/scribble/g;
  s/\bSatellite/Heavenly Body/g;
  s/\bsatellite/heavenly body/g;
  s/\bSnapshot/Etching/g;
  s/\bsnapshot/etching/g;
  #s/\bObject//g;
  #s/\bobject//g;
  #s/\bItem//g;
  #s/\bitem//g;
  s/\bInterface/Grapple/g;
  s/\binterface/grapple/g;
  s/\bConfigure/Provision me' ship/g;
  s/\bconfigure/provision me' ship/g;

  # From userscripts:
  s/About/\'bout/g;
  s/\babout\b/\'bout/g;
  s/\ba lot\b/mightily/g;
  s/\bam\b/be/g;
  s/\bamputee\b/peg leg/g;
  s/\bafraid\b/lily-livered/g;
  s/\band\b/an\'/g;
  s/\baround\b/\'round/g;
  s/\battack\b/pillage/g;
  s/\battacked\b/raped and pillaged/g;
  s/\barrest\b/keelhaul/g;
  s/\bAIDS\b/scurvy/g;
  s/\baids\b/scurvy/g;
  s/\bATTN\b/AVAST/g;

  s/\bbad\b/scurvy/g;
  s/\bbeer\b/grog/g;
  s/\bvodka\b/grog/g;
  s/\bban him\b/make him walk the plank/g;
  s/\bcar\b/ship/g;
  s/\bBAN HIM\b/Make him walk the plank!/g;
  s/\bBan him\b/Make him walk the plank/g;

  s/\bale\b/grog/g;
  s/\bbetween\b/betwixt/g;
  s/\bwhiskey\b/grog/g;
  s/\bbeauty\b/gov\'nor\'s daughter/g;
  s/\bbefore\b/\'ere/g;
  s/\bbanned\b/forced t\' walk the plank/g;
  s/\bbetween\b/\'tween/g;
  s/\bboy\b/jim lad/g;
  s/\bboys\b/jim lads/g;
  s/\bbought\b/pilfered/g;
  s/\b4chan\b/House o\' Bilge Rats/g;

  s/\bAsia\b/Th\' Mystic East/g;
  s/\bJapan\b/Th\' Mystic East/g;
  s/\bChina\b/Th\' Mystic East/g;
  s/\bKorea\b/Th\' Mystic East/g;
  s/\basia\b/Th\' Mystic East/g;
  s/\bjapan\b/Th\' Mystic East/g;
  s/\bchina\b/Th\' Mystic East/g;
  s/\bkorea\b/Th\' Mystic East/g;
  s/\bIndia\b/Hindustan/g;
  s/\bIsrael\b/Th\' Holy Lands/g;
  s/\bindia\b/Hindustan/g;
  s/\bisrael\b/Th\' Holy Lands/g;
  s/\bIraq\b/Th\' Ottoman Empire/g;
  s/\bIran\b/Th\' Ottoman Empire/g;
  s/\bPakistan\b/Th\' Ottoman Empire/g;
  s/\bAfghanistan\b/Th\' Ottoman Empire/g;
  s/\biraq\b/Th\' Ottoman Empire/g;
  s/\biran\b/Th\' Ottoman Empire/g;
  s/\bpakistan\b/Th\' Ottoman Empire/g;
  s/\bafghanistan\b/Th\' Ottoman Empire/g;
  s/\bAfrica\b/Th\' Dark Continent/g;
  s/\bafrica\b/Th\' Dark Continent/g;

  s/\bcheat\b/hornswaggle/g;
  s/\bchild\b/wee one/g;
  s/\bchildren\b/wee ones/g;
  s/\bcoffee\b/grog/g;
  s/\bcondemn\b/keelhaul/g;

  s/\bconference\b/parlay/g;
  s/\bcrazy\b/addled/g;
  s/\bjapanophile\b/scurvy mutt/g;
  s/\bweeaboo\b/scurvy mutt/g;
  s/\boh crap\b/shiver me timbers!/g;
  s/\bover\b/o\'er/g; 
  s/\bThe Token Shop\b/Honest Jack\'s Swag Shop/g; 
  s/\bToken Shop\b/Honest Jack\'s Swag Shop/g; 

  s/\bdamn\b/damn\'ed/g;    
  s/\bdevil\b/Davy Jones/g;    
  s/\bdie\b/head to Davy Jones\' Locker/g;  
  s/\bdead\b/\'n Davy Jones\' Locker/g;
  s/\bdoesn\'t\b/don\'t/g;
  s/\bdollars\b/pieces o\' eight/g;
  s/\beveryone\b/all hands/g;
  s/\beyewear\b/eye patch/g;
  s/\bglasses\b/eye patches/g;
  s/\bfight\b/duel/g;
  s/\bgreatly\b/mightily/g;
  s/\bgold\b/dubloons/g;
  s/\bha\b/har har/g;
  s/\bhaha\b/har har/g;
  s/\bbase\b/port/g;
  s/\bfort\b/port/g;
  s/\bhah\b/har har/g;
  s/\bheh\b/har har/g;
  s/\bHa\b/Har har/g;
  s/\bflag\b/Jolly Roger/g;
  s/\bhouse\b/shanty/g;
  s/\bidiot\b/bilge rat/g;

  s/\bhit\b/flog/g;
  s/\btorrents\b/Blackbeard\'s treasure /g;
  s/\btorrent\b/Blackbeard\'s treasure/g;

  s/\bn00b\b/landlubber/g; 
  s/\bnoob\b/landlubber/g; 
  s/\btroll\b/blowhard/g;
  s/\bdrive\b/sail/g;
  s/\bcoins\b/pieces o\' eight/g;
  s/\bcorrect\b/right an\' true/g;

  s/\bfly\b/sail/g;
  s/\bfool\b/squiffy/g;
  s/\bfoolish\b/addled/g;
  s/\bfor\b/fer/g;
  s/\bFor\b/Fer/g;
  s/\bfriend\b/matey/g;
  s/\bfriends\b/hearties/g;
  s/\bgirl\b/lass/g;
  s/\bex-girlfriend\b/festerin\' harlot/g;
  s/\bex girlfriend\b/festerin\' harlot/g;
  s/\bgood\b/worthy/g;
  s/\byou\'re\b/yer/g;  
  s/\byour\b/yer/g; 

  s/\bhello\b/ahoy/g;
  s/\bHello\b/Ahoy/g;
  s/\bhey\b/avast!/g;
  s/\bHey\b/Avast/g;
  s/\bhey\b/avast!/g;
  s/\bhi\b/ahoy/g; 
  s/\bHi\b/Ahoy/g; 
  s/\bHiya\b/Ahoy/g;   
  s/\bhiya\b/ahoy/g;   
  s/\bmoney\b/booty/g; 
  s/\bguy\b/feller/g;  
  s/\bfellow\b/feller/g;
  s/\bidiot\b/scalawag/g;
  s/ing\b/in\'/g;
  s/\bin\b/\'n/g;
  s/\bis\b/be/g;
  s/\bit\'s\b/\'tis/g;
  s/\bit is\b/\'tis/g;
  s/\bkid\b/wee one/g;
  s/\bkids\b/wee ones/g;
  s/\bkill\b/keelhaul/g;

  s/\bis not\b/be not/g;
  s/\baren\'t\b/be not/g;
  s/\bare\b/be/g;
  s/\bam\b/be/g;
  s/\bAre\b/Be/g;
  s/\blol\b/yo ho ho!/g;
  s/\blolol\b/Me sides be splittin\'!/g;
  s/\bodd\b/addled/g;
  s/\bof\b/o\'/g;
  s/\bohmigod\b/begad!/g;
  s/\bomigod\b/begad!/g;
  s/\bomg\b/begad!/g;
  s/\bOMG\b/BEGAD!/g;
  s/\bo rly\b/be that right, sailor?/g;
  s/\borly\b/be that right, sailor?/g;
  s/\bya rly\b/Sailor, \'tis true/g;
  s/\byarly\b/Sailor, \'tis true/g;
  s/\bwhoamg\b/shiver me timbers!/g;

  s/\bmoney\b/booty/g;
  s/\bmy\b/me/g;
  s/\bprosecute\b/keelhaul/g;
  s/\bpants\b/britches/g;
  s/\bHello\b/Ahoy!/g; 
  s/\bquick\b/smart/g;
  s/\bquickly\b/smartly/g;
  s/\bthe rules\b/the Pirate\'s Code/g;
  s/\bnice\b/fine/g;
  s/\bthe Internet\b/The Seven Seas/g;
  s/\bThe Internet\b/The Seven Seas/g;
  s/\binternet\b/Seven Seas/g;
  s/\bInternet\b/Seven Seas/g;

  s/\bsilly\b/addled/g;
  s/\bsword\b/cutlass/g;
  s/\bshe\b/the lass/g;
  s/\bshut up\b/pipe down/g;
  #s/\bspeech/parlance/g;
  #s/\bSpeech/Parlance/g;
  s/\bspeech/parley/g;
  s/\bSpeech/Parley/g;
  s/\bsteal\b/commandeer/g;
  s/\bdownload\b/plunder/g;
  s/\bDownload\b/Plunder/g;

  s/\bsexy\b/saucy/g;
  s/\btelescope\b/spyglass/g;
  s/\bterrorist\b/scourge o\' the seven seas/g;
  s/\bterrorists\b/scalawags/g;
  s/tion\b/tin\'/g;
  s/\bthere\b/thar/g;
  s/tions\b/tin\'s/g;
  s/\bto\b/t\'/g;
  s/\btomorrow\b/the morrow/g;
  s/\btruck\b/vessel/g;
    
  s/\bwasn\'t\b/weren\'t/g;
  s/\bwant to\b/wish t\'/g;
  s/\bwanna\b/wish t\'/g;
  s/\bYep\b/Aye/g; 
  s/\byep\b/aye/g; 
  s/\bwoman\b/buxom beauty/g;
  s/\bwomen\b/wenches/g;
  s/\bwin\b/triumph/g;
  s/\bwins\b/triumphs/g;
  s/\bwork\b/deck swabbing/g;
  s/\bwine\b/grog/g;
  s/\byes\b/aye/g;
  s/\bYes\b/Aye/g;
  s/\bno\b/nay/g;
  s/\bNo\b/Nay/g;
  s/\bnah\b/nay/g;
  s/\bNah\b/Nay/g;
  s/\bYeah\b/Aye/g;
  s/\byeah\b/Aye/g;

  s/\byou\b/ye/g;
  s/\byour\b/yer/g;
  s/\bwtf\b/what devilry!/g;
  s/\bWTF\b/Begad, what devilry be is?!/g;
  s/\bFacebook\b/PirateBook/g;
  s/\bThis message was deleted at the request of the original poster\b/This here message be taken back by a yellow-bellied pirate/g;
  s/\bThis message has been deleted by a moderator\b/This poor soul had a run-in with the authorities/g;
  s/\bGood Tokens\b/Pieces o\' Eight/g;
  s/\bBad Tokens\b/Black Marks/g;
  s/\byou\'re\b/yer/g;
  s/\bYou\'re\b/Yer/g;
  s/\bwins\b/triumphs/g;   

  s/\bHome\b/Haven/g;
  s/\bAdd a link\b/Add plunder/g;
  s/\bRandom link\b/Who needs maps?/g;
  s/\bTop rated links\b/Quality grog/g;
  s/\bLinks o\' the week\b/Modern fashions/g;
  s/\bWiki\b/Wikis/g;
  s/\bAll links\b/All th\' plunder/g;
  s/\bFavorites\b/Treasures/g;
  s/\bSearch\b/Scour/g;
  s/\bStats\b/Specs/g;
  s/\bBoards\b/Th\' Tavern/g;
  s/\bUser List\b/Roster/g;
  s/\bLogout\b/Retreat/g;
  s/\bHelp\b/Aid/g;
  s/\bBoard List\b/Port/g;
  s/\bCreate New Topic\b/Parley/g;
  s/\bPost New Message\b/Parley/g;
  s/\bNext Page\b/Next Map/g;
  s/\bTagged\b/X\'d/g;
  s/\bTag\b/X/g;

  # From text-filter-suite/
  s/\bmy\b/me/g;
  s/\bboss\b/admiral/g;
  s/\bmanager\b/admiral/g;
  s/\b[Cc]aptain\b/Cap\'n/g;
  s/\bmyself\b/meself/g;
  s/\byour\b/yer/g;
  s/\byou\b/ye/g;
  s/\bfriend\b/matey/g;
  s/\bfriends\b/maties/g;
  s/\bco[-]?worker\b/shipmate/g;
  s/\bco[-]?workers\b/shipmates/g;
  s/\bpeople\b/scallywags/g;
  s/\bearlier\b/afore/g;
  s/\bold\b/auld/g;
  s/\bthe\b/th\'/g;
  s/\bof\b/o'/g;
  s/\bdon\'t\b/dern\'t/g;
  s/\bdo not\b/dern\'t/g;
  s/\bnever\b/ne\'er/g;
  s/\bever\b/e\'er/g;
  s/\bover\b/o\'er/g;
  s/\bYes\b/Aye/g;
  s/\bNo\b/Nay/g;
  s/\bYeah\b/Aye/g;
  s/\byeah\b/aye/g;
  s/\bdon\'t know\b/dinna/g;
  s/\bdidn\'t know\b/did nay know/g;
  s/\bhadn\'t\b/ha\'nae/g;
  s/\bdidn\'t\b/di\'nae/g;
  s/\bwasn\'t\b/weren\'t/g;
  s/\bhaven\'t\b/ha\'nae/g;
  s/\bfor\b/fer/g;
  s/\bbetween\b/betwixt/g;
  s/\baround\b/aroun\'/g;
  s/\bto\b/t\'/g;
  s/\bit\'s\b/\'tis/g;
  s/\bwoman\b/wench/g;
  s/\bwomen\b/wenches/g;
  s/\blady\b/wench/g;
  s/\bwife\b/lady/g;
  s/\bgirl\b/lass/g;
  s/\bgirls\b/lassies/g;
  s/\bguy\b/lubber/g;
  s/\bman\b/lubber/g;
  s/\bfellow\b/lubber/g;
  s/\bdude\b/lubber/g;
  s/\bboy\b/lad/g;
  s/\bboys\b/laddies/g;
  s/\bchildren\b/little sandcrabs/g;
  s/\bkids\b/minnows/g;
  s/\bhim\b/that scurvey dog/g;
  s/\bher\b/that comely wench/g;
  s/\bhim\.\b/that drunken sailor/g;
  s/\bHe\b/The ornery cuss/g;
  s/\bShe\b/The winsome lass/g;
  s/\bhe\'s\b/he be/g;
  s/\bshe\'s\b/she be/g;
  s/\bwas\b/were bein\'/g;
  s/\bHey\b/Avast/g;
  s/\bher\.\b/that lovely lass/g;
  s/\bfood\b/chow/g;
  s/\bmoney\b/dubloons/g;
  s/\bdollars\b/pieces of eight/g;
  s/\bcents\b/shillings/g;
  s/\broad\b/sea/g;
  s/\broads\b/seas/g;
  s/\bstreet\b/river/g;
  s/\bstreets\b/rivers/g;
  s/\bhighway\b/ocean/g;
  s/\bhighways\b/oceans/g;
  s/\binterstate\b/high sea/g;
  s/\bprobably\b/likely/g;
  s/\bidea\b/notion/g;
  s/\bcar\b/boat/g;
  s/\bcars\b/boats/g;
  s/\btruck\b/schooner/g;
  s/\btrucks\b/schooners/g;
  s/\bSUV\b/ship/g;
  s/\bairplane\b/flying machine/g;
  s/\bjet\b/flying machine/g;
  s/\bmachine\b/contraption/g;
  s/\bdriving\b/sailing/g;
  s/\bunderstand\b/reckon/g;
  s/\bdrive\b/sail/g;
  s/\bdied\b/snuffed it/g;
  s/ing\b/in\'/g;
  s/ings\b/in\'s/g;

  # These next two do cool random substitutions
  #s/(\.\s)/e/avast("$0",3)/g;
  #s/([!\?]\s)/e/avast("$0",2)/g; # Greater chance after exclamation


# Add an opening phrase to each line randomly (see below)?


  if ($do_split) {
    # Combine the line again for output to STDOUT
    $pieces[1] = $_;
    print join '|', @pieces;
  }
  else {
    print;
  }
}


# Randomize use of this array, both in order and in frequency of
# use.  Not currently used at all.
#my @shouts = (
#  ", avast$stub",
#  "$stub Ahoy!",
#  ", and a bottle of rum!",
#  ", by Blackbeard's sword$stub",
#  ", by Davy Jones' locker$stub",
#  "$stub Walk the plank!",
#  "$stub Aarrr!",
#  "$stub Yaaarrrrr!",
#  ", pass the grog!",
#  ", and dinna spare the whip!",
#  ", with a chest full of booty$stub",
#  ", and a bucket o' chum$stub",
#  ", we'll keel-haul ye!",
#  "$stub Shiver me timbers!",
#  "$stub And hoist the mainsail!",
#  "$stub And swab the deck!",
#  ", ye scurvey dog$stub",
#  "$stub Fire the cannons!",
#  ", to be sure$stub",
#  ", I'll warrant ye$stub",
#  ", on a dead man's chest!",
#  "$stub Load the cannons!",
#  "$stub Prepare to be boarded!",
#  ", I'll warrant ye$stub",
#  "$stub Ye'll be sleepin' with the fishes!",
#  "$stub The sharks will eat well tonight!",
#  "$stub Oho!",
#  "$stub Fetch me spyglass!",
# );


# Randomize use of this array both in order and frequency of use.
# Not currently used at all.
my @openings = (
  'Avast! ',
  'Yarrr! ',
  'Blimey! ',
  'Ahoy! ',
  'Harrr! ',
  'Aye aye! ',
  'Shiver me timbers! ',
  'Arrrr! '
);


