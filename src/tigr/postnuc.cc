
//-- NOTE: this option will significantly hamper program performance,
//         mostly the alignment extension performance (sw_align.h)
//#define _DEBUG_ASSERT       // self testing assert functions

#include <vector>
#include <algorithm>
#include <iostream>
#include <fstream>
#include <iomanip>
#include <limits>
#include <memory>

#include "postnuc.hh"
#include "tigrinc.hh"
#include "sw_align.hh"

namespace mummer {
namespace postnuc {
// Read one sequence from fasta stream into T (starting at index 1, 0
// is unused), store its name and returns true if successful.
bool Read_Sequence(std::istream& is, std::string& T, std::string& name) {
  int c = is.peek();
  for( ; c != EOF && c != '>'; c = is.peek())
    ignore_line(is);
  if(c == EOF) return false;
  std::getline(is, name);
  name = name.substr(1, name.find_first_of(" \t\n") - 1);

  T = '\0';
  std::string line;
  for(c = is.peek(); c != EOF && c != '>'; c = is.peek()) {
    c = is.get();
    if(isspace(c)) continue;
    c = tolower(c);
    if(!isalpha(c) && c != '*')
      c = 'x';
    T += c;
  }
  return true;
}

bool Read_Sequence(std::istream& is, FastaRecord& record) {
  if(!Read_Sequence(is, record.m_seq, record.m_Id)) return false;
  record.m_len = record.m_seq.size() - 1;
  return true;
}

bool merge_syntenys::extendBackward(std::vector<Alignment> & Alignments, std::vector<Alignment>::iterator CurrAp,
                                    std::vector<Alignment>::iterator TargetAp, const char * A, const char * B)

//  Extend an alignment backwards off of the current alignment object.
//  The current alignment object must be freshly created and consist
//  only of an exact match (i.e. the delta vector MUST be empty).
//  If the TargetAp alignment object is reached by the extension, it will
//  be merged with CurrAp and CurrAp will be destroyed. If TargetAp is
//  NULL the function will extend as far as possible. It is a strange
//  and dangerous function because it can delete CurrAp, so edit with
//  caution. Returns true if TargetAp was reached and merged, else false
//  Designed only as a subroutine for extendClusters, should be used
//  nowhere else.

{
  bool target_reached = false;
  bool overflow_flag  = false;
  bool double_flag    = false;

  std::vector<long int>::iterator Dp;

  unsigned int m_o;
  long int targetA, targetB;

  m_o = BACKWARD_SEARCH;

  //-- Set the target coordinates
  if ( TargetAp != Alignments.end( ) )
    {
      targetA = TargetAp->eA;
      targetB = TargetAp->eB;
    }
  else
    {
      targetA = 1;
      targetB = 1;
      m_o |= OPTIMAL_BIT;
    }

  //-- If alignment is too long, bring with bounds and set overflow_flag true
  if ( CurrAp->sA - targetA + 1 > MAX_ALIGNMENT_LENGTH )
    {
      targetA = CurrAp->sA - MAX_ALIGNMENT_LENGTH + 1;
      overflow_flag = true;
      m_o |= OPTIMAL_BIT;
    }
  if ( CurrAp->sB - targetB + 1 > MAX_ALIGNMENT_LENGTH )
    {
      targetB = CurrAp->sB - MAX_ALIGNMENT_LENGTH + 1;
      if ( overflow_flag )
        double_flag = true;
      else
        overflow_flag = true;
      m_o |= OPTIMAL_BIT;
    }


  if ( TO_SEQEND && !double_flag )
    m_o |= SEQEND_BIT;


  //-- Attempt to reach the target
  target_reached = alignSearch (A, CurrAp->sA, targetA,
                                B, CurrAp->sB, targetB, m_o);

  if ( overflow_flag  ||  TargetAp == Alignments.end( ) )
    target_reached = false;

  if ( target_reached )
    {
      //-- Merge the two alignment objects
      extendForward (TargetAp, A, CurrAp->sA,
                     B, CurrAp->sB, FORCED_FORWARD_ALIGN);
      TargetAp->eA = CurrAp->eA;
      TargetAp->eB = CurrAp->eB;
      Alignments.pop_back( );
    }
  else
    {
      alignTarget (A, targetA, CurrAp->sA,
                   B, targetB, CurrAp->sB,
                   CurrAp->delta, FORCED_FORWARD_ALIGN);
      CurrAp->sA = targetA;
      CurrAp->sB = targetB;

      //-- Update the deltaApos value for the alignment object
      for ( Dp = CurrAp->delta.begin( ); Dp < CurrAp->delta.end( ); Dp ++ )
        CurrAp->deltaApos += *Dp > 0 ? *Dp : labs(*Dp) - 1;
    }

  return target_reached;
}




bool extendForward
(std::vector<Alignment>::iterator CurrAp, const char * A, long int targetA,
 const char * B, long int targetB, unsigned int m_o)

//  Extend an alignment forwards off the current alignment object until
//  target or end of sequence is reached, and merge the delta values of the
//  alignment object with the new delta values generated by the extension.
//  Return true if the target was reached, else false

{
  long int ValA;
  long int ValB;
  unsigned int Di;
  bool target_reached;
  bool overflow_flag = false;
  bool double_flag = false;
  std::vector<long int>::iterator Dp;

  //-- Set Di to the end of the delta vector
  Di = CurrAp->delta.size( );

  //-- Calculate the distance between the start and end positions
  ValA = targetA - CurrAp->eA + 1;
  ValB = targetB - CurrAp->eB + 1;

  //-- If the distance is too long, shrink it and set the overflow_flag
  if ( ValA > MAX_ALIGNMENT_LENGTH )
    {
      targetA = CurrAp->eA + MAX_ALIGNMENT_LENGTH - 1;
      overflow_flag = true;
      m_o |= OPTIMAL_BIT;
    }
  if ( ValB > MAX_ALIGNMENT_LENGTH )
    {
      targetB = CurrAp->eB + MAX_ALIGNMENT_LENGTH - 1;
      if ( overflow_flag )
        double_flag = true;
      else
        overflow_flag = true;
      m_o |= OPTIMAL_BIT;
    }

  if ( double_flag )
    m_o &= ~SEQEND_BIT;

  target_reached = alignTarget (A, CurrAp->eA, targetA,
                                B, CurrAp->eB, targetB,
                                CurrAp->delta, m_o);

  //-- Notify user if alignment was chopped short
  if ( target_reached  &&  overflow_flag )
    target_reached = false;

  //-- Pick up delta where left off (Di) and merge with new deltas
  if ( Di < CurrAp->delta.size( ) )
    {
      //-- Merge the deltas
      ValA = (CurrAp->eA - CurrAp->sA + 1) - CurrAp->deltaApos - 1;
      CurrAp->delta[Di] += CurrAp->delta[Di] > 0 ? ValA : -(ValA);
      if ( CurrAp->delta[Di] == 0  ||  ValA < 0 ) {
        cerr << "ERROR: failed to merge alignments at position " << CurrAp->eA << '\n'
             << "       Please file a bug report\n";
        exit (EXIT_FAILURE);
      }

      //-- Update the deltaApos
      for ( Dp = CurrAp->delta.begin( ) + Di; Dp < CurrAp->delta.end( ); Dp ++ )
        CurrAp->deltaApos += *Dp > 0 ? *Dp : labs(*Dp) - 1;
    }

  //-- Set the alignment coordinates
  CurrAp->eA = targetA;
  CurrAp->eB = targetB;

  return target_reached;
}

void printDeltaAlignments(const std::vector<Alignment> & Alignments,
                          const FastaRecord * Af, const FastaRecord * Bf,
                          std::ostream& DeltaFile)

//  Simply output the delta information stored in Alignments to the
//  given delta file. Free the memory used by Alignments once the
//  data is successfully output to the file.

{
  DeltaFile << '>' << Af->Id() << ' ' << Bf->Id() << ' ' << Af->len() << ' ' << Bf->len() << '\n';

  for(const auto& A : Alignments) {
    const bool fwd = A.dirB == FORWARD_CHAR;
    DeltaFile << A.sA << ' ' << A.eA << ' '
              << (fwd ? A.sB : revC(A.sB, Bf->len())) << ' '
              << (fwd ? A.eB : revC(A.eB, Bf->len())) << ' '
              << A.Errors << ' ' << A.SimErrors << ' ' << A.NonAlphas
              << '\n';

    for(const auto& D : A.delta)
      DeltaFile << D << '\n';
    DeltaFile << "0\n";
  }
}




void printSyntenys(const std::vector<Synteny> & Syntenys, const FastaRecord& Bf, std::ostream& ClusterFile)

//  Simply output the synteny/cluster information generated by the mgaps
//  program. However, now the coordinates reference their appropriate
//  reference sequence, and the reference sequecne header is added to
//  the appropriate lines. Free the memory used by Syntenys once the
//  data is successfully output to the file.

{
  if ( ClusterFile ) {
    for(const auto& Sp : Syntenys) { // each syntenys
      ClusterFile << '>' << Sp.AfP->Id() << ' ' << Bf.Id() << ' '
                  << Sp.AfP->len() << ' ' << Bf.len() << '\n';

      for (const auto& Cp : Sp.clusters) { // each clusters
        ClusterFile << setw(2) << FORWARD_CHAR << ' ' << setw(2) << Cp.dirB << '\n';

        for (auto Mp = Cp.matches.cbegin( ); Mp != Cp.matches.cend( ); ++Mp ) { // each match
            ClusterFile << setw(8) << Mp->sA << ' '
                        << setw(8) << (Cp.dirB == FORWARD_CHAR ? Mp->sB : revC(Mp->sB, Bf.len())) << ' '
                        << setw(6) << Mp->len;
          if ( Mp != Cp.matches.cbegin( ) )
            ClusterFile << setw(6) << (Mp->sA - (Mp - 1)->sA - (Mp - 1)->len) << ' '
                        << setw(6) << (Mp->sB - (Mp - 1)->sB - (Mp - 1)->len) << '\n';
          else
            ClusterFile << "     -      -\n";
        }
      }
    }
  }
}




std::vector<Cluster>::iterator getForwardTargetCluster
(std::vector<Cluster> & Clusters, std::vector<Cluster>::iterator CurrCp,
 long int & targetA, long int & targetB)

//  Return the cluster that is most likely to successfully join (in a
//  forward direction) with the current cluster. The returned cluster
//  must contain 1 or more matches that are strictly greater than the end
//  of the current cluster. The targeted cluster must also be on a
//  diagonal close enough to the current cluster, so that a connection
//  could possibly be made by the alignment extender. Assumes clusters
//  have been sorted via AscendingClusterSort. Returns targeted cluster
//  and stores the target coordinates in targetA and targetB. If no
//  suitable cluster was found, the function will return NULL and target
//  A and targetB will remain unchanged.

{
  std::vector<Match>::iterator Mip;               // match iteratrive pointer
  std::vector<Cluster>::iterator Cp;              // cluster pointer
  std::vector<Cluster>::iterator Cip;             // cluster iterative pointer
  long int eA, eB;                           // possible target
  long int greater, lesser;                  // gap sizes between two clusters
  long int sA = CurrCp->matches.rbegin( )->sA +
    CurrCp->matches.rbegin( )->len - 1;      // the endA of the current cluster 
  long int sB = CurrCp->matches.rbegin( )->sB +
    CurrCp->matches.rbegin( )->len - 1;      // the endB of the current cluster

  //-- End of sequences is the default target, set distance accordingly
  long int dist = (targetA - sA < targetB - sB ? targetA - sA : targetB - sB);

  //-- For all clusters greater than the current cluster (on sequence A)
  Cp = Clusters.end( );
  for ( Cip = CurrCp + 1; Cip < Clusters.end( ); Cip ++ )
    {
      //-- If the cluster is on the same direction
      if ( CurrCp->dirB == Cip->dirB )
        {
          eA = Cip->matches.begin( )->sA;
          eB = Cip->matches.begin( )->sB;

          //-- If the cluster overlaps the current cluster, strip some matches
          if ( ( eA < sA  ||  eB < sB )  &&
               Cip->matches.rbegin( )->sA >= sA  &&
               Cip->matches.rbegin( )->sB >= sB )
            {
              for ( Mip = Cip->matches.begin( );
                    Mip < Cip->matches.end( )  &&  ( eA < sA  ||  eB < sB );
                    Mip ++ )
                {
                  eA = Mip->sA;
                  eB = Mip->sB;
                }
            }

          //-- If the cluster is strictly greater than current cluster
          if ( eA >= sA  &&  eB >= sB )
            {
              if ( eA - sA > eB - sB )
                {
                  greater = eA - sA;
                  lesser = eB - sB;
                }
              else
                {
                  lesser = eA - sA;
                  greater = eB - sB;
                }

              //-- If the cluster is close enough
              if ( greater < getBreakLen( )  ||
                   (lesser) * GOOD_SCORE [getMatrixType( )] +
                   (greater - lesser) * CONT_GAP_SCORE [getMatrixType( )] >= 0 )
                {
                  Cp = Cip;
                  targetA = eA;
                  targetB = eB;
                  break;
                }
              else if ( (greater << 1) - lesser < dist )
                {
                  Cp = Cip;
                  targetA = eA;
                  targetB = eB;
                  dist = (greater << 1) - lesser;
                }
            }
        }
    }


  return Cp;
}




std::vector<Alignment>::iterator getReverseTargetAlignment
(std::vector<Alignment> & Alignments, std::vector<Alignment>::iterator CurrAp)

//  Return the alignment that is most likely to successfully join (in a
//  reverse direction) with the current alignment. The returned alignment
//  must be strictly less than the current cluster and be on a diagonal
//  close enough to the current alignment, so that a connection
//  could possibly be made by the alignment extender. Assumes clusters
//  have been sorted via AscendingClusterSort and processed in order, so
//  therefore all alignments are in order by their start A coordinate.

{
  std::vector<Alignment>::iterator Ap;        // alignment pointer
  std::vector<Alignment>::iterator Aip;       // alignment iterative pointer
  long int eA, eB;                       // possible targets
  long int greater, lesser;              // gap sizes between the two alignments
  long int sA = CurrAp->sA;              // the startA of the current alignment
  long int sB = CurrAp->sB;              // the startB of the current alignment

  //-- Beginning of sequences is the default target, set distance accordingly
  long int dist = (sA < sB ? sA : sB);

  //-- For all alignments less than the current alignment (on sequence A)
  Ap = Alignments.end( );
  for ( Aip = CurrAp - 1; Aip >= Alignments.begin( ); Aip -- )
    {
      //-- If the alignment is on the same direction
      if ( CurrAp->dirB == Aip->dirB )
        {
          eA = Aip->eA;
          eB = Aip->eB;

          //-- If the alignment is strictly less than current cluster
          if ( eA <= sA  && eB <= sB )
            {
              if ( sA - eA > sB - eB )
                {
                  greater = sA - eA;
                  lesser = sB - eB;
                }
              else
                {
                  lesser = sA - eA;
                  greater = sB - eB;
                }

              //-- If the cluster is close enough
              if ( greater < getBreakLen( )  ||
                   (lesser) * GOOD_SCORE [getMatrixType( )] +
                   (greater - lesser) * CONT_GAP_SCORE [getMatrixType( )] >= 0 )
                {
                  Ap = Aip;
                  break;
                }
              else if ( (greater << 1) - lesser < dist )
                {
                  Ap = Aip;
                  dist = (greater << 1) - lesser;
                }
            }
        }
    }


  return Ap;
}




bool isShadowedCluster
(std::vector<Cluster>::iterator CurrCp,
 std::vector<Alignment> & Alignments, std::vector<Alignment>::iterator Ap)

//  Check if the current cluster is shadowed by a previously produced
//  alignment region. Return true if it is, else false.

{
  std::vector<Alignment>::iterator Aip;         // alignment pointer

  long int sA = CurrCp->matches.begin( )->sA;
  long int eA = CurrCp->matches.rbegin( )->sA +
    CurrCp->matches.rbegin( )->len - 1;
  long int sB = CurrCp->matches.begin( )->sB;
  long int eB = CurrCp->matches.rbegin( )->sB +
    CurrCp->matches.rbegin( )->len - 1;

  if ( ! Alignments.empty( ) )             // if there are alignments to use
    {
      //-- Look backwards in hope of finding a shadowing alignment
      for ( Aip = Ap; Aip >= Alignments.begin( ); Aip -- )
        {
          //-- If in the same direction and shadowing the current cluster, break
          if ( Aip->dirB == CurrCp->dirB )
            if ( Aip->eA >= eA  &&  Aip->eB >= eB )
              if ( Aip->sA <= sA  &&  Aip->sB <= sB )
                break;
        }

      //-- Return true if the loop was broken, i.e. shadow found
      if ( Aip >= Alignments.begin( ) )
        return true;
    }

  //-- Return false if Alignments was empty or loop was not broken
  return false;
}




void __parseAbort
(const char * s, const char* file, size_t line)

//  Abort the program if there was an error in parsing file 's'

{
  std::cerr << "ERROR: " << file << ':' << line
            << " Could not parse input from '" << s << "'. \n"
            << "Please check the filename and format, or file a bug report\n";
  exit (EXIT_FAILURE);
}

void parseDelta
(std::vector<Alignment> & Alignments,
 const FastaRecord * Af, const FastaRecord *Bf)

// Use the delta information to generate the error counts for each
// alignment, and fill this information into the data type

{
  const char * A, * B;
  std::unique_ptr<char[]> Brev;
  char ch1, ch2;
  long int Delta;
  int Sign;
  long int i, Apos, Bpos;
  long int Remain, Total;
  long int Errors, SimErrors;
  long int NonAlphas;
  std::vector<Alignment>::iterator Ap;
  std::vector<long int>::iterator Dp;

  for ( Ap = Alignments.begin( ); Ap != Alignments.end( ); ++Ap) {
      A = Af->seq();
      B = Bf->seq();

      if ( Ap->dirB == REVERSE_CHAR ) {
        if (!Brev) {
          Brev.reset(new char[Bf->len() + 2]);
          strcpy ( Brev.get() + 1, Bf->seq() + 1 );
          Brev[0] = '\0';
          Reverse_Complement (Brev.get(), 1, Bf->len());
        }
        B = Brev.get();
      }

      Apos = Ap->sA;
      Bpos = Ap->sB;

      Errors = 0;
      SimErrors = 0;
      NonAlphas = 0;
      Remain = Ap->eA - Ap->sA + 1;
      Total = Remain;

      //-- For all delta's in this alignment
      for ( Dp = Ap->delta.begin( ); Dp != Ap->delta.end( ); ++Dp) {
        Delta = *Dp;
        Sign = Delta > 0 ? 1 : -1;
        Delta = std::abs ( Delta );

        //-- For all the bases before the next indel
          for ( i = 1; i < Delta; i ++ ) {
            ch1 = A [Apos ++];
            ch2 = B [Bpos ++];

            if ( !isalpha (ch1) ) {
              ch1 = STOP_CHAR;
              NonAlphas ++;
            }
            if ( !isalpha (ch2) ) {
              ch2 = STOP_CHAR;
              NonAlphas ++;
            }

            ch1 = toupper(ch1);
            ch2 = toupper(ch2);
            if (1 > MATCH_SCORE[getMatrixType( )][ch1 - 'A'][ch2 - 'A'] )
              SimErrors ++;
            if ( ch1 != ch2 )
                Errors ++;
          }

          //-- Process the current indel
          Remain -= i - 1;
          Errors ++;
          SimErrors ++;

          if ( Sign == 1 ) {
            Apos ++;
            Remain --;
          } else {
            Bpos ++;
            Total ++;
          }
      }

      //-- For all the bases after the final indel
      for ( i = 0; i < Remain; i ++ ) {
        //-- Score character match and update error counters
        ch1 = A [Apos ++];
        ch2 = B [Bpos ++];

        if ( !isalpha (ch1) ) {
          ch1 = STOP_CHAR;
          NonAlphas ++;
        }
        if ( !isalpha (ch2) ) {
          ch2 = STOP_CHAR;
          NonAlphas ++;
        }

        ch1 = toupper(ch1);
        ch2 = toupper(ch2);
        if ( 1 > MATCH_SCORE[getMatrixType( )][ch1 - 'A'][ch2 - 'A'] )
          SimErrors ++;
        if ( ch1 != ch2 )
          Errors ++;
      }

      Ap->Errors = Errors;
      Ap->SimErrors = SimErrors;
      Ap->NonAlphas = NonAlphas;
  }
}


inline long int revC
(long int Coord, long int Len)

//  Reverse complement the given coordinate for the given length.

{
  assert (Len - Coord + 1 > 0);
  return (Len - Coord + 1);
}

// always_assert: similar to assert macro, but not subject to NDEBUG
#define always_assert(x)                                                \
  if(!(x)) {                                                            \
    std::cerr << __FILE__ << ':' << __LINE__                            \
              << ": assertion failed " << #x << std::endl;              \
      abort();                                                          \
  }

void validateData
(std::vector<Alignment> Alignments, std::vector<Cluster> Clusters,
 const FastaRecord * Af, const FastaRecord * Bf)

//  Self test function to check that the cluster and alignment information
//  is valid

{
  std::unique_ptr<char[]> Brev;
  std::vector<Cluster>::iterator   Cp;
  std::vector<Match>::iterator     Mp;
  std::vector<Alignment>::iterator Ap;
  const char *                     A = Af->seq(), * B;

  for ( Cp = Clusters.begin( ); Cp < Clusters.end( ); Cp ++ ) {
    always_assert ( Cp->wasFused );

    //-- Pick the right directional sequence for B
    if ( Cp->dirB == FORWARD_CHAR ) {
      B = Bf->seq();
    } else if ( Brev ) {
      B = Brev.get();
    } else {
      Brev.reset(new char[Bf->len() + 2]);
      strcpy ( Brev.get() + 1, Bf->seq() + 1 );
      Brev[0] = '\0';
      Reverse_Complement (Brev.get(), 1, Bf->len());
      B = Brev.get();
    }

    for ( Mp = Cp->matches.begin( ); Mp < Cp->matches.end( ); ++Mp) {
      //-- always_assert for each match in cluster, it is indeed a match
      long int x = Mp->sA;
      long int y = Mp->sB;
      for (long int i = 0; i < Mp->len; i ++ )
        always_assert ( A[x ++] == B[y ++] );

      //-- always_assert for each match in cluster, it is contained in an alignment
      for ( Ap = Alignments.begin( ); Ap < Alignments.end( ); Ap ++ ) {
        if ( Ap->sA <= Mp->sA  &&  Ap->sB <= Mp->sB  &&
             Ap->eA >= Mp->sA + Mp->len - 1  &&
             Ap->eB >= Mp->sB + Mp->len - 1 )
          break;
      }
      always_assert ( Ap < Alignments.end( ) );
    }
  }

  //-- always_assert alignments are optimal (quick check if first and last chars equal)
  for ( Ap = Alignments.begin( ); Ap < Alignments.end( ); ++Ap) {
    if ( Ap->dirB == REVERSE_CHAR ) {
      always_assert (Brev);
      B = Brev.get();
    } else
      B = Bf->seq();
    always_assert ( Ap->sA <= Ap->eA );
    always_assert ( Ap->sB <= Ap->eB );

    always_assert ( Ap->sA >= 1 && Ap->sA <= Af->len() );
    always_assert ( Ap->eA >= 1 && Ap->eA <= Af->len() );
    always_assert ( Ap->sB >= 1 && Ap->sB <= Bf->len() );
    always_assert ( Ap->eB >= 1 && Ap->eB <= Bf->len() );

    char Xc = toupper(isalpha(A[Ap->sA]) ? A[Ap->sA] : STOP_CHAR);
    char Yc = toupper(isalpha(B[Ap->sB]) ? B[Ap->sB] : STOP_CHAR);
    always_assert ( 0 <= MATCH_SCORE [0] [Xc - 'A'] [Yc - 'A'] );

    Xc = toupper(isalpha(A[Ap->eA]) ? A[Ap->eA] : STOP_CHAR);
    Yc = toupper(isalpha(B[Ap->eB]) ? B[Ap->eB] : STOP_CHAR);
    always_assert ( 0 <= MATCH_SCORE [0] [Xc - 'A'] [Yc - 'A'] );
  }
}

} // namespace postnuc
} // namespace mummer
