/*******************************************************************************
*                                                                              *
* rangeset.c         -- Nirvana Editor rangest functions                       *
*                                                                              *
* Copyright (C) 1999 Mark Edel                                                 *
*                                                                              *
* This is free software; you can redistribute it and/or modify it under the    *
* terms of the GNU General Public License as published by the Free Software    *
* Foundation; either version 2 of the License, or (at your option) any later   *
* version. In addition, you may distribute version of this program linked to   *
* Motif or Open Motif. See README for details.                                 *
*                                                                              *
* This software is distributed in the hope that it will be useful, but WITHOUT *
* ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or        *
* FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License        *
* for more details.                                                            *
*                                                                              *
* You should have received a copy of the GNU General Public License along with *
* software; if not, write to the Free Software Foundation, Inc., 59 Temple     *
* Place, Suite 330, Boston, MA        02111-1307 USA                           *
*                                                                              *
* Nirvana Text Editor                                                          *
* Sep 26, 2002                                                                 *
*                                                                              *
* Written by Tony Balinski with contributions from Andrew Hood                 *
*                                                                              *
* Modifications:                                                               *
*                                                                              *
*                                                                              *
*******************************************************************************/

#include "Rangeset.h"
#include "RangesetTable.h"
#include "TextBuffer.h"
#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace {

// -------------------------------------------------------------------------- 

RangesetUpdateFn rangesetInsDelMaintain;
RangesetUpdateFn rangesetInclMaintain;
RangesetUpdateFn rangesetDelInsMaintain;
RangesetUpdateFn rangesetExclMaintain;
RangesetUpdateFn rangesetBreakMaintain;

#define DEFAULT_UPDATE_FN_NAME "maintain"

struct {
    const char *name;
	RangesetUpdateFn *update_fn;
} RangesetUpdateMap[] = {
    {DEFAULT_UPDATE_FN_NAME, rangesetInsDelMaintain},
    {"ins_del", rangesetInsDelMaintain},
    {"include", rangesetInclMaintain},
    {"del_ins", rangesetDelInsMaintain},
    {"exclude", rangesetExclMaintain},
    {"break", rangesetBreakMaintain}
};


// -------------------------------------------------------------------------- 

bool is_start(int i) {
    return !(i & 1);
}

bool is_end(int i) {
    return (i & 1);
}

void rangesetRefreshAllRanges(Rangeset *rangeset) {
    for (int i = 0; i < rangeset->n_ranges_; i++) {
        rangeset->RangesetRefreshRange(rangeset->ranges_[i].start, rangeset->ranges_[i].end);
	}
}


// -------------------------------------------------------------------------- 

/*
** Find the index of the first integer in table greater than or equal to pos.
** Fails with len (the total number of entries). The start index base can be
** chosen appropriately.
*/

// TODO(eteran): is this just an implementation of std::lower_bound?
int at_or_before(int *table, int base, int len, int val) {
	int lo, mid = 0, hi;

	if (base >= len)
		return len; // not sure what this means! 

	lo = base;    // first valid index 
	hi = len - 1; // last valid index 

	while (lo <= hi) {
		mid = (lo + hi) / 2;
		if (val == table[mid])
			return mid;
		if (val < table[mid])
			hi = mid - 1;
		else
			lo = mid + 1;
	}
	// if we get here, we didn't find val itself 
	if (val > table[mid])
		mid++;

	return mid;
}

int weighted_at_or_before(int *table, int base, int len, int val) {
	int lo, mid = 0, hi;
	int min, max;

	if (base >= len)
		return len; // not sure what this means! 

	lo = base;    // first valid index 
	hi = len - 1; // last valid index 

	min = table[lo]; // establish initial min/max 
	max = table[hi];

	if (val <= min) // initial range checks 
		return lo;  // needed to avoid out-of-range mid values 
	else if (val > max)
		return len;
	else if (val == max)
		return hi;

	while (lo <= hi) {
		// Beware of integer overflow when multiplying large numbers! 
		mid = lo + (int)((hi - lo) * (double)(val - min) / (max - min));
		// we won't worry about min == max - values should be unique 

		if (val == table[mid])
			return mid;
		if (val < table[mid]) {
			hi = mid - 1;
			max = table[mid];
		} else { // val > table[mid] 
			lo = mid + 1;
			min = table[mid];
		}
	}

	// if we get here, we didn't find val itself 
	if (val > table[mid])
		return mid + 1;

	return mid;
}

// -------------------------------------------------------------------------- 


Rangeset *rangesetFixMaxpos(Rangeset *rangeset, int ins, int del) {
    rangeset->maxpos_ += ins - del;
	return rangeset;
}

// -------------------------------------------------------------------------- 


/*
** Find the index of the first entry in the range set's ranges table (viewed as
** an int array) whose value is equal to or greater than pos. As a side effect,
** update the lasi_index value of the range set. Return's the index value. This
** will be twice p->n_ranges if pos is beyond the end.
*/

int rangesetWeightedAtOrBefore(Rangeset *rangeset, int pos) {
	int i;
	int last;
    auto rangeTable = reinterpret_cast<int *>(rangeset->ranges_);

    int n = rangeset->n_ranges_;
	if (n == 0)
		return 0;

    last = rangeset->last_index_;

	if (last >= n || last < 0)
		last = 0;

	n *= 2;
	last *= 2;

	if (pos >= rangeTable[last])                             // ranges[last_index].start 
		i = weighted_at_or_before(rangeTable, last, n, pos); // search end only 
	else
		i = weighted_at_or_before(rangeTable, 0, last, pos); // search front only 

    rangeset->last_index_ = i / 2;

	return i;
}

/*
** Adjusts values in tab[] by an amount delta, perhaps moving them meanwhile.
*/

int rangesetShuffleToFrom(int *rangeTable, int to, int from, int n, int delta) {
	int end, diff = from - to;

	if (n <= 0)
		return 0;

	if (delta != 0) {
		if (diff > 0) { // shuffle entries down 
			for (end = to + n; to < end; to++)
				rangeTable[to] = rangeTable[to + diff] + delta;
		} else if (diff < 0) { // shuffle entries up 
			for (end = to, to += n; --to >= end;)
				rangeTable[to] = rangeTable[to + diff] + delta;
		} else { // diff == 0: just run through 
			for (end = n; end--;)
				rangeTable[to++] += delta;
		}
	} else {
		if (diff > 0) { // shuffle entries down 
			for (end = to + n; to < end; to++)
				rangeTable[to] = rangeTable[to + diff];
		} else if (diff < 0) { // shuffle entries up 
			for (end = to, to += n; --to >= end;)
				rangeTable[to] = rangeTable[to + diff];
		}
		// else diff == 0: nothing to do 
	}

	return n;
}

/*
** Functions to adjust a rangeset to include new text or remove old.
** *** NOTE: No redisplay: that's outside the responsability of these routines.
*/

/* "Insert/Delete": if the start point is in or at the end of a range
** (start < pos && pos <= end), any text inserted will extend that range.
** Insertions appear to occur before deletions. This will never add new ranges.
*/

Rangeset *rangesetInsDelMaintain(Rangeset *rangeset, int pos, int ins, int del) {

    auto rangeTable = reinterpret_cast<int *>(rangeset->ranges_);

    int n = 2 * rangeset->n_ranges_;

	int i = rangesetWeightedAtOrBefore(rangeset, pos);

	if (i == n)
		return rangesetFixMaxpos(rangeset, ins, del); // all beyond the end 

	int end_del = pos + del;
	int movement = ins - del;

	/* the idea now is to determine the first range not concerned with the
	   movement: its index will be j. For indices j to n-1, we will adjust
	   position by movement only. (They may need shuffling up or down, depending
	   on whether ranges have been deleted or created by the change.) */
	int j = i;
	while (j < n && rangeTable[j] <= end_del) // skip j to first ind beyond changes 
		j++;

	/* if j moved forward, we have deleted over rangeTable[i] - reduce it accordingly,
	   accounting for inserts. */
	if (j > i)
		rangeTable[i] = pos + ins;

	/* If i and j both index starts or ends, just copy all the rangeTable[] values down
	   by j - i spaces, adjusting on the way. Otherwise, move beyond rangeTable[i]
	   before doing this. */

	if (is_start(i) != is_start(j))
		i++;

	rangesetShuffleToFrom(rangeTable, i, j, n - j, movement);

	n -= j - i;
    rangeset->n_ranges_ = n / 2;
    rangeset->ranges_ = RangesetTable::RangesRealloc(rangeset->ranges_, rangeset->n_ranges_);

	// final adjustments 
	return rangesetFixMaxpos(rangeset, ins, del);
}

/* "Inclusive": if the start point is in, at the start of, or at the end of a
** range (start <= pos && pos <= end), any text inserted will extend that range.
** Insertions appear to occur before deletions. This will never add new ranges.
** (Almost identical to rangesetInsDelMaintain().)
*/

Rangeset *rangesetInclMaintain(Rangeset *rangeset, int pos, int ins, int del) {
	int j;
    auto rangeTable = reinterpret_cast<int *>(rangeset->ranges_);

    int n = 2 * rangeset->n_ranges_;

	int i = rangesetWeightedAtOrBefore(rangeset, pos);

	if (i == n)
		return rangesetFixMaxpos(rangeset, ins, del); // all beyond the end 

	/* if the insert occurs at the start of a range, the following lines will
	   extend the range, leaving the start of the range at pos. */

    if (is_start(i) && rangeTable[i] == pos && ins > 0) {
		i++;
    }

    int end_del = pos + del;
    int movement = ins - del;

	/* the idea now is to determine the first range not concerned with the
	   movement: its index will be j. For indices j to n-1, we will adjust
	   position by movement only. (They may need shuffling up or down, depending
	   on whether ranges have been deleted or created by the change.) */
	j = i;
    while (j < n && rangeTable[j] <= end_del) { // skip j to first ind beyond changes
		j++;
    }

	/* if j moved forward, we have deleted over rangeTable[i] - reduce it accordingly,
	   accounting for inserts. */
    if (j > i) {
		rangeTable[i] = pos + ins;
    }

	/* If i and j both index starts or ends, just copy all the rangeTable[] values down
	   by j - i spaces, adjusting on the way. Otherwise, move beyond rangeTable[i]
	   before doing this. */
    if (is_start(i) != is_start(j)) {
		i++;
    }

	rangesetShuffleToFrom(rangeTable, i, j, n - j, movement);

	n -= j - i;
    rangeset->n_ranges_ = n / 2;
    rangeset->ranges_ = RangesetTable::RangesRealloc(rangeset->ranges_, rangeset->n_ranges_);

	// final adjustments 
	return rangesetFixMaxpos(rangeset, ins, del);
}

/* "Delete/Insert": if the start point is in a range (start < pos &&
** pos <= end), and the end of deletion is also in a range
** (start <= pos + del && pos + del < end) any text inserted will extend that
** range. Deletions appear to occur before insertions. This will never add new
** ranges.
*/

Rangeset *rangesetDelInsMaintain(Rangeset *rangeset, int pos, int ins, int del) {
	int j;
    auto rangeTable = reinterpret_cast<int *>(rangeset->ranges_);
	int end_del, movement;

    int n = 2 * rangeset->n_ranges_;

	int i = rangesetWeightedAtOrBefore(rangeset, pos);

	if (i == n)
		return rangesetFixMaxpos(rangeset, ins, del); // all beyond the end 

	end_del = pos + del;
	movement = ins - del;

	/* the idea now is to determine the first range not concerned with the
	   movement: its index will be j. For indices j to n-1, we will adjust
	   position by movement only. (They may need shuffling up or down, depending
	   on whether ranges have been deleted or created by the change.) */
	j = i;
	while (j < n && rangeTable[j] <= end_del) // skip j to first ind beyond changes 
		j++;

	/* if j moved forward, we have deleted over rangeTable[i] - reduce it accordingly,
	   accounting for inserts. (Note: if rangeTable[j] is an end position, inserted
	   text will belong to the range that rangeTable[j] closes; otherwise inserted
	   text does not belong to a range.) */
	if (j > i)
		rangeTable[i] = (is_end(j)) ? pos + ins : pos;

	/* If i and j both index starts or ends, just copy all the rangeTable[] values down
	   by j - i spaces, adjusting on the way. Otherwise, move beyond rangeTable[i]
	   before doing this. */

	if (is_start(i) != is_start(j))
		i++;

	rangesetShuffleToFrom(rangeTable, i, j, n - j, movement);

	n -= j - i;
    rangeset->n_ranges_ = n / 2;
    rangeset->ranges_ = RangesetTable::RangesRealloc(rangeset->ranges_, rangeset->n_ranges_);

	// final adjustments 
	return rangesetFixMaxpos(rangeset, ins, del);
}

/* "Exclusive": if the start point is in, but not at the end of, a range
** (start < pos && pos < end), and the end of deletion is also in a range
** (start <= pos + del && pos + del < end) any text inserted will extend that
** range. Deletions appear to occur before insertions. This will never add new
** ranges. (Almost identical to rangesetDelInsMaintain().)
*/

Rangeset *rangesetExclMaintain(Rangeset *rangeset, int pos, int ins, int del) {
	int j;
    auto rangeTable = reinterpret_cast<int *>(rangeset->ranges_);
	int end_del, movement;

    int n = 2 * rangeset->n_ranges_;

	int i = rangesetWeightedAtOrBefore(rangeset, pos);

	if (i == n)
		return rangesetFixMaxpos(rangeset, ins, del); // all beyond the end 

	/* if the insert occurs at the end of a range, the following lines will
	   skip the range, leaving the end of the range at pos. */

	if (is_end(i) && rangeTable[i] == pos && ins > 0)
		i++;

	end_del = pos + del;
	movement = ins - del;

	/* the idea now is to determine the first range not concerned with the
	   movement: its index will be j. For indices j to n-1, we will adjust
	   position by movement only. (They may need shuffling up or down, depending
	   on whether ranges have been deleted or created by the change.) */
	j = i;
	while (j < n && rangeTable[j] <= end_del) // skip j to first ind beyond changes 
		j++;

	/* if j moved forward, we have deleted over rangeTable[i] - reduce it accordingly,
	   accounting for inserts. (Note: if rangeTable[j] is an end position, inserted
	   text will belong to the range that rangeTable[j] closes; otherwise inserted
	   text does not belong to a range.) */
	if (j > i)
		rangeTable[i] = (is_end(j)) ? pos + ins : pos;

	/* If i and j both index starts or ends, just copy all the rangeTable[] values down
	   by j - i spaces, adjusting on the way. Otherwise, move beyond rangeTable[i]
	   before doing this. */

	if (is_start(i) != is_start(j))
		i++;

	rangesetShuffleToFrom(rangeTable, i, j, n - j, movement);

	n -= j - i;
    rangeset->n_ranges_ = n / 2;
    rangeset->ranges_ = RangesetTable::RangesRealloc(rangeset->ranges_, rangeset->n_ranges_);

	// final adjustments 
	return rangesetFixMaxpos(rangeset, ins, del);
}

/* "Break": if the modification point pos is strictly inside a range, that range
** may be broken in two if the deletion point pos+del does not extend beyond the
** end. Inserted text is never included in the range.
*/

Rangeset *rangesetBreakMaintain(Rangeset *rangeset, int pos, int ins, int del) {
	int j;
    auto rangeTable = reinterpret_cast<int *>(rangeset->ranges_);
	int end_del, movement, need_gap;

    int n = 2 * rangeset->n_ranges_;

	int i = rangesetWeightedAtOrBefore(rangeset, pos);

	if (i == n)
		return rangesetFixMaxpos(rangeset, ins, del); // all beyond the end 

	/* if the insert occurs at the end of a range, the following lines will
	   skip the range, leaving the end of the range at pos. */

	if (is_end(i) && rangeTable[i] == pos && ins > 0)
		i++;

	end_del = pos + del;
	movement = ins - del;

	/* the idea now is to determine the first range not concerned with the
	   movement: its index will be j. For indices j to n-1, we will adjust
	   position by movement only. (They may need shuffling up or down, depending
	   on whether ranges have been deleted or created by the change.) */
	j = i;
	while (j < n && rangeTable[j] <= end_del) // skip j to first ind beyond changes 
		j++;

	if (j > i)
		rangeTable[i] = pos;

	// do we need to insert a gap? yes if pos is in a range and ins > 0 

	/* The logic for the next statement: if i and j are both range ends, range
	   boundaries indicated by index values between i and j (if any) have been
	   "skipped". This means that rangeTable[i-1],rangeTable[j] is the current range. We will
	   be inserting in that range, splitting it. */

	need_gap = (is_end(i) && is_end(j) && ins > 0);

	// if we've got start-end or end-start, skip rangeTable[i] 
	if (is_start(i) != is_start(j)) { // one is start, other is end 
		if (is_start(i)) {
			if (rangeTable[i] == pos)
				rangeTable[i] = pos + ins; // move the range start 
		}
		i++; // skip to next index 
	}

	/* values rangeTable[j] to rangeTable[n-1] must be adjusted by movement and placed in
	   position. */

	if (need_gap)
		i += 2; // make space for the break 

	// adjust other position values: shuffle them up or down if necessary 
	rangesetShuffleToFrom(rangeTable, i, j, n - j, movement);

	if (need_gap) { // add the gap informations 
		rangeTable[i - 2] = pos;
		rangeTable[i - 1] = pos + ins;
	}

	n -= j - i;
    rangeset->n_ranges_ = n / 2;
    rangeset->ranges_ = RangesetTable::RangesRealloc(rangeset->ranges_, rangeset->n_ranges_);

	// final adjustments 
	return rangesetFixMaxpos(rangeset, ins, del);
}

// -------------------------------------------------------------------------- 

}

/*
** Return the name, if any.
*/

QString Rangeset::RangesetGetName() const {
    return name_;
}

/*
** Find out whether the position pos is included in one of the ranges of
** rangeset. Returns the containing range's index if true, -1 otherwise.
** Note: ranges are indexed from zero.
*/

int Rangeset::RangesetFindRangeNo(int index, int *start, int *end) const {
    if (index < 0 || n_ranges_ <= index || !ranges_) {
		return 0;
	}

    *start = ranges_[index].start;
    *end   = ranges_[index].end;

	return 1;
}

/*
** Find out whether the position pos is included in one of the ranges of
** rangeset. Returns the containing range's index if true, -1 otherwise.
** Note: ranges are indexed from zero.
*/

int Rangeset::RangesetFindRangeOfPos(int pos, int incl_end) const {

    if (!n_ranges_ || !ranges_) {
		return -1;
	}
	
    auto ranges = reinterpret_cast<int *>(ranges_); // { s1,e1, s2,e2, s3,e3,... }
    int len = n_ranges_ * 2;
	int ind = at_or_before(ranges, 0, len, pos);

	if (ind == len)
		return -1; // beyond end 

	if (ind & 1) { // ind odd: references an end marker 
		if (pos < ranges[ind] || (incl_end && pos == ranges[ind]))
			return ind / 2; // return the range index 
	} else {                // ind even: references start marker 
		if (pos == ranges[ind])
			return ind / 2; // return the range index 
	}
	return -1; // not in any range 
}

/*
** Return the color validity, if any, and the value in *color.
*/

int Rangeset::RangesetGetColorValid(QColor *color) const {
	*color = color_;
    return color_set_;
}

/*
** Get number of ranges in rangeset.
*/

int Rangeset::RangesetGetNRanges() const {
    return n_ranges_;
}


/*
** Invert the rangeset (replace it with its complement in the range 0-maxpos).
** Returns the number of ranges if successful, -1 otherwise. Never adds more
** than one range.
*/

int Rangeset::RangesetInverse() {
	int n;

    auto rangeTable = reinterpret_cast<int *>(ranges_);

    if (n_ranges_ == 0) {
		if (!rangeTable) {
            ranges_ = RangesetTable::RangesNew(1);
            rangeTable = reinterpret_cast<int *>(ranges_);
		}
		rangeTable[0] = 0;
        rangeTable[1] = maxpos_;
		n = 2;
	} else {
        n = n_ranges_ * 2;

		// find out what we have 
		bool has_zero = (rangeTable[0] == 0);
        bool has_end = (rangeTable[n - 1] == maxpos_);

		// fill the entry "beyond the end" with the buffer's length 
        rangeTable[n + 1] = rangeTable[n] = maxpos_;

		if (has_zero) {
			// shuffle down by one 
			rangesetShuffleToFrom(rangeTable, 0, 1, n, 0);
			n -= 1;
		} else {
			// shuffle up by one 
			rangesetShuffleToFrom(rangeTable, 1, 0, n, 0);
			rangeTable[0] = 0;
			n += 1;
		}
		if (has_end)
			n -= 1;
		else
			n += 1;
	}

    n_ranges_ = n / 2;
    ranges_ = RangesetTable::RangesRealloc(reinterpret_cast<Range *>(rangeTable), n_ranges_);

    RangesetRefreshRange(0, maxpos_);
    return n_ranges_;
}

/*
** Merge the ranges in rangeset plusSet into rangeset this.
*/

int Rangeset::RangesetAdd(Rangeset *plusSet) {
	Range *oldRanges;

	Range *origRanges = ranges_;
	int nOrigRanges   = n_ranges_;

	Range *plusRanges = plusSet->ranges_;
	int nPlusRanges   = plusSet->n_ranges_;

	if (nPlusRanges == 0) {
		return nOrigRanges; // no ranges in plusSet - nothing to do 
	}

	Range *newRanges = RangesetTable::RangesNew(nOrigRanges + nPlusRanges);

	if (nOrigRanges == 0) {
		// no ranges in destination: just copy the ranges from the other set 
		memcpy(newRanges, plusRanges, nPlusRanges * sizeof(Range));
        RangesetTable::RangesFree(ranges_);
        ranges_ = newRanges;
        n_ranges_ = nPlusRanges;
		for (nOrigRanges = 0; nOrigRanges < nPlusRanges; nOrigRanges++) {
			RangesetRefreshRange(newRanges->start, newRanges->end);
			newRanges++;
		}
		return nPlusRanges;
	}

	oldRanges = origRanges;
    ranges_ = newRanges;
    n_ranges_ = 0;

	/* in the following we merrily swap the pointers/counters of the two input
	   ranges (from this and plusSet) - don't worry, they're both consulted
	   read-only - building the merged set in newRanges */

	bool isOld = true; // true if origRanges points to a range in oldRanges[]

	while (nOrigRanges > 0 || nPlusRanges > 0) {
		// make the range with the lowest start value the origRanges range 
		if (nOrigRanges == 0 || (nPlusRanges > 0 && origRanges->start > plusRanges->start)) {
			std::swap(origRanges, plusRanges);
			std::swap(nOrigRanges, nPlusRanges);
			isOld = !isOld;
		}

        n_ranges_++; // we're using a new result range

		*newRanges = *origRanges++;
		nOrigRanges--;
		if (!isOld)
			RangesetRefreshRange(newRanges->start, newRanges->end);

		// now we must cycle over plusRanges, merging in the overlapped ranges 
		while (nPlusRanges > 0 && newRanges->end >= plusRanges->start) {
			do {
				if (newRanges->end < plusRanges->end) {
					if (isOld)
						RangesetRefreshRange(newRanges->end, plusRanges->end);
					newRanges->end = plusRanges->end;
				}
				plusRanges++;
				nPlusRanges--;
			} while (nPlusRanges > 0 && newRanges->end >= plusRanges->start);

			/* by now, newRanges->end may have extended to overlap more ranges in origRanges,
			   so swap and start again */
			std::swap(origRanges, plusRanges);
			std::swap(nOrigRanges, nPlusRanges);
			isOld = !isOld;
		}

		/* OK: now *newRanges holds the result of merging all the first ranges from
		   origRanges and plusRanges - now we have a break in contiguity, so move on to the
		   next newRanges in the result */
		newRanges++;
	}

	// finally, forget the old rangeset values, and reallocate the new ones 
	RangesetTable::RangesFree(oldRanges);
    ranges_ = RangesetTable::RangesRealloc(ranges_, n_ranges_);

    return n_ranges_;
}

/*
** Add the range indicated by the positions start and end. Returns the
** new number of ranges in the set.
*/

int Rangeset::RangesetAddBetween(int start, int end) {
	int i, j;
    auto rangeTable = (int *)ranges_;

	if (start > end) {
		// quietly sort the positions 
		std::swap(start, end);
	} else if (start == end) {
        return n_ranges_; // no-op - empty range == no range
	}

    int n = 2 * n_ranges_;

	if (n == 0) { // make sure we have space 
        ranges_ = RangesetTable::RangesNew(1);
        rangeTable = (int *)ranges_;
		i = 0;
	} else
		i = rangesetWeightedAtOrBefore(this, start);

	if (i == n) { // beyond last range: just add it 
		rangeTable[n] = start;
		rangeTable[n + 1] = end;
        n_ranges_++;
        ranges_ = RangesetTable::RangesRealloc(ranges_, n_ranges_);

		RangesetRefreshRange(start, end);
        return n_ranges_;
	}

	j = i;
	while (j < n && rangeTable[j] <= end) // skip j to first ind beyond changes 
		j++;

	if (i == j) {
		if (is_start(i)) {
			// is_start(i): need to make a gap in range rangeTable[i-1], rangeTable[i] 
			rangesetShuffleToFrom(rangeTable, i + 2, i, n - i, 0); // shuffle up 
			rangeTable[i] = start;                                 // load up new range's limits 
			rangeTable[i + 1] = end;
            n_ranges_++; // we've just created a new range
            ranges_ = RangesetTable::RangesRealloc(ranges_, n_ranges_);
		} else {
            return n_ranges_; // no change
		}
	} else {
		// we'll be shuffling down 
		if (is_start(i))
			rangeTable[i++] = start;
		if (is_start(j))
			rangeTable[--j] = end;
		if (i < j)
			rangesetShuffleToFrom(rangeTable, i, j, n - j, 0);
		n -= j - i;
        n_ranges_ = n / 2;
        ranges_ = RangesetTable::RangesRealloc(ranges_, n_ranges_);
	}

	RangesetRefreshRange(start, end);
    return n_ranges_;
}

/*
** Assign a color name to a rangeset via the rangeset table.
*/

bool Rangeset::RangesetAssignColorName(const std::string &color_name) {


    // "" invalid
	if(!color_name.empty()) {
		color_name_ = QString::fromStdString(color_name);
    } else {
        color_name_ = QString();
    }

    color_set_ = 0;

	rangesetRefreshAllRanges(this);
	return true;
}

/*
** Assign a color pixel value to a rangeset via the rangeset table. If ok is
** false, the color_set flag is set to an invalid (negative) value.
*/

bool Rangeset::RangesetAssignColorPixel(const QColor &color, int ok) {
    color_set_ = ok ? 1 : -1;
	color_ = color;
	return true;
}


/*
** Assign a name to a rangeset via the rangeset table.
*/

bool Rangeset::RangesetAssignName(const std::string &name) {

	// store new name value 
    if(!name.empty()) {
        name_ = QString::fromStdString(name);
	} else {
        name_ = QString();
	}

	return true;
}


/*
** Change a range set's modification behaviour. Returns true (non-zero)
** if the update function name was found, else false.
*/

bool Rangeset::RangesetChangeModifyResponse(const char *name) {

    if(!name) {
		name = DEFAULT_UPDATE_FN_NAME;
    }

	for(auto &entry : RangesetUpdateMap) {
        if (strcmp(entry.name, name) == 0) {
            update_fn_   = entry.update_fn;
            update_name_ = entry.name;
			return true;
		}
	}

	return false;
}


/*
** Find out whether the position pos is included in one of the ranges of
** rangeset. Returns the containing range's index if true, -1 otherwise.
** Essentially the same as the RangesetFindRangeOfPos() function, but uses the
** last_index member of the rangeset and weighted_at_or_before() for speedy
** lookup in refresh tasks. The rangeset is assumed to be valid, as is the
** position. We also don't allow checking of the endpoint.
** Returns the including range index, or -1 if not found.
*/

int Rangeset::RangesetCheckRangeOfPos(int pos) {

	int index;

    int len = n_ranges_;
	if (len == 0)
		return -1; // no ranges 

    auto ranges = (int *)ranges_; // { s1,e1, s2,e2, s3,e3,... }
    int last = last_index_;

	// try to profit from the last lookup by using its index 
	if (last >= len || last < 0) {
		last = (len > 0) ? len - 1 : 0; // make sure last is in range 
        last_index_ = last;
	}

	len *= 2;
	last *= 2;

	if (pos >= ranges[last]) {      // last even: this is a start 
		if (pos < ranges[last + 1]) // checking an end here 
            return last / 2;        // no need to change last_index_
		else
			last += 2; // not in this range: move on 

		if (last == len)
			return -1; // moved on too far 

		// find the entry in the upper portion of ranges 
		index = weighted_at_or_before(ranges, last, len, pos); // search end only 
	} else if (last > 0) {
		index = weighted_at_or_before(ranges, 0, last, pos); // search front only 
	} else
		index = 0;

    last_index_ = index / 2;

	if (index == len)
		return -1; // beyond end 

	if (index & 1) { // index odd: references an end marker 
		if (pos < ranges[index])
			return index / 2; // return the range index 
	} else {                  // index even: references start marker 
		if (pos == ranges[index])
			return index / 2; // return the range index 
	}
	return -1; // not in any range 
}


/*
** Subtract the ranges of minusSet from this rangeset .
*/

int Rangeset::RangesetRemove(Rangeset *minusSet) {
	Range *origRanges, *minusRanges, *newRanges, *oldRanges;
	int nOrigRanges, nMinusRanges;

    origRanges = ranges_;
    nOrigRanges = n_ranges_;

    minusRanges = minusSet->ranges_;
    nMinusRanges = minusSet->n_ranges_;

	if (nOrigRanges == 0 || nMinusRanges == 0)
		return 0; // no ranges in this or minusSet - nothing to do 

	// we must provide more space: each range in minusSet might split a range in this 
    newRanges = RangesetTable::RangesNew(n_ranges_ + minusSet->n_ranges_);

	oldRanges = origRanges;
    ranges_ = newRanges;
    n_ranges_ = 0;

	/* consider each range in origRanges - we do not change any of minusRanges's data, but we
	   may change origRanges's - it will be discarded at the end */

	while (nOrigRanges > 0) {
		do {
			// skip all minusRanges ranges strictly in front of *origRanges 
			while (nMinusRanges > 0 && minusRanges->end <= origRanges->start) {
				minusRanges++; // *minusRanges in front of *origRanges: move onto next *minusRanges 
				nMinusRanges--;
			}

			if (nMinusRanges > 0) {
				// keep all origRanges ranges strictly in front of *minusRanges 
				while (nOrigRanges > 0 && origRanges->end <= minusRanges->start) {
					*newRanges++ = *origRanges++; // *minusRanges beyond *origRanges: save *origRanges in *newRanges 
					nOrigRanges--;
                    n_ranges_++;
				}
			} else {
				// no more minusRanges ranges to remove - save the rest of origRanges 
				while (nOrigRanges > 0) {
					*newRanges++ = *origRanges++;
					nOrigRanges--;
                    n_ranges_++;
				}
			}
		} while (nMinusRanges > 0 && minusRanges->end <= origRanges->start); // any more non-overlaps 

		// when we get here either we're done, or we have overlap 
		if (nOrigRanges > 0) {
			if (minusRanges->start <= origRanges->start) {
				// origRanges->start inside *minusRanges 
				if (minusRanges->end < origRanges->end) {
					RangesetRefreshRange(origRanges->start, minusRanges->end);
					origRanges->start = minusRanges->end; // cut off front of original *origRanges 
					minusRanges++;                        // dealt with this *minusRanges: move on 
					nMinusRanges--;
				} else {
					// all *origRanges inside *minusRanges 
					RangesetRefreshRange(origRanges->start, origRanges->end);
					origRanges++; // all of *origRanges can be skipped 
					nOrigRanges--;
				}
			} else {
				// minusRanges->start inside *origRanges: save front, adjust or skip rest 
				newRanges->start = origRanges->start; // save front of *origRanges in *newRanges 
				newRanges->end = minusRanges->start;
				newRanges++;
                n_ranges_++;

				if (minusRanges->end < origRanges->end) {
					// all *minusRanges inside *origRanges 
					RangesetRefreshRange(minusRanges->start, minusRanges->end);
					origRanges->start = minusRanges->end; // cut front of *origRanges upto end *minusRanges 
					minusRanges++;                        // dealt with this *minusRanges: move on 
					nMinusRanges--;
				} else {
					// minusRanges->end beyond *origRanges 
					RangesetRefreshRange(minusRanges->start, origRanges->end);
					origRanges++; // skip rest of *origRanges 
					nOrigRanges--;
				}
			}
		}
	}

	// finally, forget the old rangeset values, and reallocate the new ones 
	RangesetTable::RangesFree(oldRanges);
    ranges_ = RangesetTable::RangesRealloc(ranges_, n_ranges_);

    return n_ranges_;
}


/*
** Remove the range indicated by the positions start and end. Returns the
** new number of ranges in the set.
*/

int Rangeset::RangesetRemoveBetween(int start, int end) {
	int j;
	
    auto rangeTable = (int *)ranges_;

	if (start > end) {
		// quietly sort the positions 
		std::swap(start, end);
	} else if (start == end) {
        return n_ranges_; // no-op - empty range == no range
	}

    int n = 2 * n_ranges_;

	int i = rangesetWeightedAtOrBefore(this, start);

	if (i == n)
        return n_ranges_; // beyond last range

	j = i;
	while (j < n && rangeTable[j] <= end) // skip j to first ind beyond changes 
		j++;

	if (i == j) {
		// removal occurs in front of rangeTable[i] 
		if (is_start(i))
            return n_ranges_; // no change
		else {
			// is_end(i): need to make a gap in range rangeTable[i-1], rangeTable[i] 
			i--;                                                   // start of current range 
			rangesetShuffleToFrom(rangeTable, i + 2, i, n - i, 0); // shuffle up 
			rangeTable[i + 1] = start;                             // change end of current range 
			rangeTable[i + 2] = end;                               // change start of new range 
            n_ranges_++;                                  // we've just created a new range
            ranges_ = RangesetTable::RangesRealloc(ranges_, n_ranges_);
		}
	} else {
		// removal occurs in front of rangeTable[j]: we'll be shuffling down 
		if (is_end(i))
			rangeTable[i++] = start;
		if (is_end(j))
			rangeTable[--j] = end;
		if (i < j)
			rangesetShuffleToFrom(rangeTable, i, j, n - j, 0);
		n -= j - i;
        n_ranges_ = n / 2;
        ranges_ = RangesetTable::RangesRealloc(ranges_, n_ranges_);
	}

	RangesetRefreshRange(start, end);
    return n_ranges_;
}




/*
** Remove all ranges from a range set.
*/

void Rangeset::RangesetEmpty() {
    Range *ranges = ranges_;

    if (!color_name_.isNull() && color_set_ > 0) {
		// this range is colored: we need to clear it 
        color_set_ = -1;

        while (n_ranges_--) {
            int start = ranges[n_ranges_].start;
            int end = ranges[n_ranges_].end;
			RangesetRefreshRange(start, end);
		}
	}

    color_name_ = QString();
    name_ = QString();
    ranges_ = RangesetTable::RangesFree(ranges_);
}

/**
 * @brief Rangeset::RangesetGetInfo
 * @return
 */
RangesetInfo Rangeset::RangesetGetInfo() const {
	RangesetInfo info;
	info.defined = true;
	info.label   = static_cast<int>(label_);
	info.count   = n_ranges_;
	info.color   = color_name_;
	info.name    = name_;
	info.mode    = update_name_;
	return info;
}

/*
** Get information about rangeset.
*/
void Rangeset::RangesetGetInfo(bool *defined, int *label, int *count, QString *color, QString *name, const char **mode) const {
    *defined = true;
	*label = static_cast<int>(label_);
    *count = n_ranges_;
	*color = color_name_;
	*name  = name_;
    *mode  = update_name_;
}

/*
** Refresh the given range on the screen. If the range indicated is null, we
** refresh the screen for the whole file.
*/

void Rangeset::RangesetRefreshRange(int start, int end) const {
    if (buf_) {
        buf_->BufCheckDisplay(start, end);
	}
}


/*
** Initialise a new range set.
*/

void Rangeset::RangesetInit(int label, TextBuffer *buf) {
	label_ = static_cast<uint8_t>(label); // a letter A-Z
    maxpos_ = 0;                   // text buffer maxpos
    last_index_ = 0;               // a place to start looking
    n_ranges_ = 0;                 // how many ranges in ranges
    ranges_ = nullptr;          // the ranges table

    color_name_ = QString();
    name_ = QString();
    color_set_ = 0;
	buf_ = buf;

	maxpos_ = buf->BufGetLength();

    RangesetChangeModifyResponse(DEFAULT_UPDATE_FN_NAME);
}
