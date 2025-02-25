#include "LeafOctree.h"
#include "util.h"
#include <string.h>
#include <algorithm>

LeafOctant::LeafOctant(vec3 min, vec3 max) {
    this->mins = min;
    this->maxs = max;
    memset(children, NULL, sizeof(LeafOctant*) * 8);
}

LeafOctant::~LeafOctant() {
    for (LeafOctant* child : children) {
        delete child;
    }
}

void LeafOctant::removeLeaf(LeafNode* leaf) {
    leaves.erase(std::remove(leaves.begin(), leaves.end(), leaf), leaves.end());
    for (int i = 0; i < 8; i++) {
        if (children[i])
            children[i]->removeLeaf(leaf);
    }
}

LeafOctree::~LeafOctree() {
    delete root;
}

LeafOctree::LeafOctree(const vec3& min, const vec3& max, int depth) {
    root = new LeafOctant(min, max);
    maxDepth = depth;
    buildOctree(root, 0);
}

void LeafOctree::buildOctree(LeafOctant* node, int currentDepth) {
    if (currentDepth >= maxDepth) {
        return;
    }
    const vec3& min = node->mins;
    const vec3& max = node->maxs;
    vec3 mid((min.x + max.x) / 2, (min.y + max.y) / 2, (min.z + max.z) / 2);

    // Define eight child octants using the min and max values
    node->children[0] = new LeafOctant(min, mid);
    node->children[1] = new LeafOctant(vec3(mid.x, min.y, min.z), vec3(max.x, mid.y, mid.z));
    node->children[2] = new LeafOctant(vec3(min.x, mid.y, min.z), vec3(mid.x, max.y, mid.z));
    node->children[3] = new LeafOctant(vec3(mid.x, mid.y, min.z), vec3(max.x, max.y, mid.z));
    node->children[4] = new LeafOctant(vec3(min.x, min.y, mid.z), vec3(mid.x, mid.y, max.z));
    node->children[5] = new LeafOctant(vec3(mid.x, min.y, mid.z), vec3(max.x, mid.y, max.z));
    node->children[6] = new LeafOctant(vec3(min.x, mid.y, mid.z), vec3(mid.x, max.y, max.z));
    node->children[7] = new LeafOctant(mid, max);

    for (LeafOctant* child : node->children) {
        buildOctree(child, currentDepth + 1);
    }
}

void LeafOctree::insertLeaf(LeafNode* leaf) {
    insertLeaf(root, leaf, 0);
}

void LeafOctree::insertLeaf(LeafOctant* node, LeafNode* leaf, int currentDepth) {
    if (currentDepth >= maxDepth) {
        node->leaves.push_back(leaf);
        return;
    }
    for (int i = 0; i < 8; ++i) {
        if (isLeafInOctant(leaf, node->children[i])) {
            insertLeaf(node->children[i], leaf, currentDepth + 1);
        }
    }
}

void LeafOctree::removeLeaf(LeafNode* leaf) {
    root->removeLeaf(leaf);
}

bool LeafOctree::isLeafInOctant(LeafNode* leaf, LeafOctant* node) {
    vec3 epsilon = vec3(1, 1, 1); // in case leaves are touching right on the border of an octree leaf
    return boxesIntersect(leaf->mins - epsilon, leaf->maxs + epsilon, node->mins, node->maxs);
}

void LeafOctree::getLeavesInRegion(LeafNode* leaf, std::vector<bool>& regionLeaves) {
    fill(regionLeaves.begin(), regionLeaves.end(), false);
    getLeavesInRegion(root, leaf, 0, regionLeaves);
}

void LeafOctree::getLeavesInRegion(LeafOctant* node, LeafNode* leaf, int currentDepth, std::vector<bool>& regionLeaves) {
    if (currentDepth >= maxDepth) {
        for (auto p : node->leaves) {
            if (p->id != -1)
                regionLeaves[p->id] = true;
        }
        return;
    }
    for (int i = 0; i < 8; ++i) {
        if (isLeafInOctant(leaf, node->children[i])) {
            getLeavesInRegion(node->children[i], leaf, currentDepth + 1, regionLeaves);
        }
    }
}
