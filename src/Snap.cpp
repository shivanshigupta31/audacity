/**********************************************************************

  Audacity: A Digital Audio Editor
  
  Snap.cpp
 
  Dominic Mazzoni

**********************************************************************/

#include <math.h>

#include "LabelTrack.h"
#include "Snap.h"
#include "TrackPanel.h"
#include "WaveTrack.h"

int CompareSnapPoints(SnapPoint *s1, SnapPoint *s2)
{
   return (s1->t - s2->t > 0? 1 : -1);
}

SnapManager::SnapManager(TrackList *tracks, TrackClipArray *exclusions,
                         double zoom, int pixelTolerance)
{
   int i;

   mSnapPoints = new SnapPointArray(CompareSnapPoints);
   if (zoom > 0 && pixelTolerance > 0)
      mTolerance = pixelTolerance / zoom;
   else {
      // This shouldn't happen, but we don't want to crash if we get
      // illegal values.  The net effect of this is to never snap.
      mTolerance = 0.0;
   }
   // Two time points closer than this are considered the same
   mEpsilon = 1 / 44100.0;

   // Add a SnapPoint at t=0
   mSnapPoints->Add(new SnapPoint(0.0, NULL));

   TrackListIterator iter(tracks);
   Track *track = iter.First();
   while (track) {
      if (track->GetKind() == Track::Label) {
         LabelTrack *labelTrack = (LabelTrack *)track;
         for(i = 0; i < labelTrack->GetNumLabels(); i++) {
            const LabelStruct *label = labelTrack->GetLabel(i);
            mSnapPoints->Add(new SnapPoint(label->t, labelTrack));
            if (label->t1 != label->t)
               mSnapPoints->Add(new SnapPoint(label->t1, labelTrack));
         }
      }
      if (track->GetKind() == Track::Wave) {
         WaveTrack *waveTrack = (WaveTrack *)track;
         WaveClipList::compatibility_iterator it;
         for (it=waveTrack->GetClipIterator(); it; it=it->GetNext()) {
            WaveClip *clip = it->GetData();
            if (exclusions) {
               bool skip = false;
               for(int j=0; j<(int)exclusions->GetCount(); j++) {
                  if ((*exclusions)[j].track == waveTrack &&
                      (*exclusions)[j].clip == clip)
                     skip = true;
               }
               if (skip)
                  continue;
            }
            mSnapPoints->Add(new SnapPoint(clip->GetStartTime(), waveTrack));
            mSnapPoints->Add(new SnapPoint(clip->GetEndTime(), waveTrack));
         }
      }

      track = iter.Next();
   }
}

SnapManager::~SnapManager()
{
   int len = (int)mSnapPoints->GetCount();
   int i;
   for(i = 0; i < len; i++)
      delete (*mSnapPoints)[i];
   delete mSnapPoints;
}

// Return the time of the SnapPoint at a given index
double SnapManager::Get(int index)
{
   return (*mSnapPoints)[index]->t;
}

// Returns the difference in time between t and the point at a given index
double SnapManager::Diff(double t, int index)
{
   return fabs(t - Get(index));
}

// Find the index where this SnapPoint should go in
// sorted order, between i0 (inclusive) and i1 (exclusive).
int SnapManager::Find(double t, int i0, int i1)
{
   if (i1 <= i0 + 1)
      return i0;

   int half = (i0 + i1) / 2;
   if (t < Get(half))
      return Find(t, i0, half);
   else
      return Find(t, half, i1);
}

// Find the SnapPoint nearest to time t
int SnapManager::Find(double t)
{
   int len = (int)mSnapPoints->GetCount();
   int index = Find(t, 0, len);

   // At this point, either index is the closest, or the next one
   // to the right is.  Keep moving to the right until we get a
   // different value
   int next = index + 1;
   while(next+1 < len && Get(next) == Get(index))
      next++;

   // Now return whichever one is closer to time t
   if (next < len && Diff(t, next) < Diff(t, index))
      return next;
   else
      return index;
}

bool SnapManager::Snap(Track *currentTrack,
                       double t,
                       bool rightEdge,
                       double *out_t)
{
   int len = (int)mSnapPoints->GetCount();
   *out_t = t;
   if (len == 0)
      return false;

   // Find the nearest SnapPoint
   int index = Find(t);

   // If it's too far away, just give up now
   if (Diff(t, index) >= mTolerance)
      return false;

   // Otherwise, search left and right for all of the points
   // within the allowed range.
   int left = index;
   int right = index;
   int i;

   while(left > 0 && Diff(t, left-1) < mTolerance)
      left--;

   while(right < len-1 && Diff(t, right+1) < mTolerance)
      right++;

   if (left == index && right == index) {
      // Awesome, there's only one point that matches!
      *out_t = Get(index);
      return true;
   }

   int indexInThisTrack = -1;
   int countInThisTrack = 0;
   for(i=left; i<=right; i++) {
      if ((*mSnapPoints)[i]->track == currentTrack) {
         indexInThisTrack = i;
         countInThisTrack++;
      }
   }
   if (countInThisTrack == 1) {
      // Cool, only one of the points is in the same track, so
      // we'll use that one.
      *out_t = Get(indexInThisTrack);
      return true;
   }

   if (Get(right) - Get(left) < mEpsilon) {
      // OK, they're basically the same point
      if (rightEdge)
         *out_t = Get(right);  // Return rightmost
      else
         *out_t = Get(left);   // Return leftmost
      return true;
   }

   // None of the points matched, bummer.
   return false;
}

// Indentation settings for Vim and Emacs.
// Please do not modify past this point.
//
// Local Variables:
// c-basic-offset: 3
// indent-tabs-mode: nil
// End:
//
// vim: et sts=3 sw=3

