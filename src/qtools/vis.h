#include "util.h"

struct BSPLEAF;

bool shiftVis(unsigned char* vis, int len, int offsetLeaf, int shift);

// decompress the given vis data into arrays of bits where each bit indicates if a leaf is visible or not
// iterationLeaves = number of leaves to decompress vis for
// visDataLeafCount = total leaves in the map (exluding the shared solid leaf 0)
// newNumLeaves = total leaves that will be in the map after merging is finished (again, excluding solid leaf 0)
void decompress_vis_lump(BSPLEAF* leafLump, unsigned char* visLump, unsigned char* output,
						 int iterationLeaves, int visDataLeafCount, int newNumLeaves, int leafMemSize = 0, int visLumpMemSize = 0);

void DecompressVis(unsigned char* src, unsigned char* dest, unsigned int dest_length, unsigned int numLeaves, unsigned int src_length = 0);

int CompressVis(unsigned char* src, unsigned int src_length, unsigned char* dest, unsigned int dest_length);

int CompressAll(BSPLEAF* leafs, unsigned char* uncompressed, unsigned char* output, int numLeaves, int iterLeaves, int bufferSize, int leafMemSize = 0);

extern bool g_debug_shift;