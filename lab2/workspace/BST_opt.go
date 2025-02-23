package main

import (
    "fmt" 
    "flag"
    "io/ioutil"
    "strings"
    "strconv"
    "time"
    "sync"
    "sync/atomic"
    "os"
    "sort"
)

//Yes my entire codebase for this lab is in one file

type IntPair struct {
    first int
    second int
}

type TreeNode struct {
    value int
    left *TreeNode
    right *TreeNode
}

type BinarySearchTree struct {
    root *TreeNode
    size int
}

func (bst *BinarySearchTree) insert(val int) {
    if bst.root == nil {
        bst.size = 1
        bst.root = &TreeNode{value: val, left: nil, right: nil}
        return
    }
    
    var curr *TreeNode = bst.root
    for true {
        if val <= curr.value {
            
            if curr.left == nil {
                curr.left = &TreeNode{value: val, left: nil, right: nil}
                break;
            } else{
                curr = curr.left;
            }

        } else {
            if curr.right == nil {
                curr.right = &TreeNode{value: val, left: nil, right: nil}
                break
            } else {
                curr = curr.right
            }
        }
    }
    
    bst.size += 1
}

//Assumes that data has enough space to store the in order traversal
//The way that it is used in the code upholds this assumption
func (bst *BinarySearchTree) PopulateInorderTraversal(data []int) {
    var index int = 0
    var myStack []*TreeNode
    var curr *TreeNode = bst.root

    for (curr != nil) || (len(myStack) > 0) {
        for curr != nil {
            myStack = append(myStack, curr)
            curr = curr.left
        }

        var currLen int = len(myStack)
        curr = myStack[currLen - 1]
        myStack = myStack[0:currLen - 1]
        data[index] = curr.value
        index++

        curr = curr.right
    }

}

func (bst *BinarySearchTree) StoreHash(hashLoc *int) {
    
    var hash int = 1
    var myStack []*TreeNode
    var curr *TreeNode = bst.root

    for (curr != nil) || (len(myStack) > 0){
    
        for curr != nil {
            myStack = append(myStack, curr)
            curr = curr.left
        }

        var currLen int = len(myStack)
        curr = myStack[currLen - 1]
        myStack = myStack[0:currLen - 1]
        
        var new_value int = curr.value + 2;
        hash = (hash * new_value + new_value) % 4222234741

        curr = curr.right

    }

    *hashLoc = hash
}


func (bst *BinarySearchTree) OrderAndHash(data *[]int) int {
    
    var hash int = 1
    var index int = 0
    var myStack []*TreeNode
    var curr *TreeNode = bst.root

    for (curr != nil) || (len(myStack) > 0){
    
        for curr != nil {
            myStack = append(myStack, curr)
            curr = curr.left
        }

        var currLen int = len(myStack)
        curr = myStack[currLen - 1]
        myStack = myStack[0:currLen - 1]
        
        (*data)[index] = curr.value
        index++

        var new_value int = curr.value + 2;
        hash = (hash * new_value + new_value) % 4222234741

        curr = curr.right
    }

    return hash
}


func (bst *BinarySearchTree) CreateTreeFromSlice(mySlice string) {
    
    var strNums []string = strings.Split(mySlice, " ")
    for _, num := range strNums {
        val, err := strconv.Atoi(num)
        if err != nil {
            fmt.Println(err)
        } else {
            (*bst).insert(val)
        }

    }

}

func compTraversals(arr1 *[]int, arr2 *[]int) bool {
    var sz int = len(*arr1)
    if sz != len(*arr2) {
        return false
    }

    for i := 0; i < sz; i++ {
        if (*arr1)[i] != (*arr2)[i] {
            return false
        }
    }

    return true
}

type CBuffer struct {
    size int
    capacity int
    producerIndex int
    consumerIndex int
    buffer []IntPair
    lock sync.Mutex
    producerCond* sync.Cond
    consumerCond* sync.Cond
}

func CBufferInit(cap int) *CBuffer{
    var myCBuf *CBuffer = &CBuffer{size: 0, capacity: cap, producerIndex: 0, consumerIndex: 0,
        buffer: make([]IntPair, cap)}
    (*myCBuf).producerCond = sync.NewCond(&(myCBuf.lock))
    (*myCBuf).consumerCond = sync.NewCond(&(myCBuf.lock))
    return myCBuf
}

func (cb *CBuffer) Put(item IntPair) {
    (*cb).lock.Lock()

    for (*cb).size == (*cb).capacity {
        //Look more into Go pointer syntax, this seems sus
        (*cb).producerCond.Wait()
    }

    (*cb).buffer[(*cb).producerIndex] = item
    (*cb).producerIndex = ((*cb).producerIndex + 1) % (*cb).capacity
    (*cb).size += 1

    (*cb).consumerCond.Signal()
    (*cb).lock.Unlock()

}

func (cb *CBuffer) Get() IntPair {
    (*cb).lock.Lock()
    
    for (*cb).size == 0 {
        (*cb).consumerCond.Wait()
    }

    var retVal IntPair = (*cb).buffer[(*cb).consumerIndex]
    (*cb).consumerIndex = ((*cb).consumerIndex + 1) % (*cb).capacity
    (*cb).size -= 1

    (*cb).producerCond.Signal()
    (*cb).lock.Unlock()

    return retVal
}

var hashWorkers int
var dataWorkers int
var compWorkers int
var inputWorkers int
var debug bool

func main() {
    var programStartTime time.Time = time.Now()

    //TODO: add more detail here
    flag.IntVar(&hashWorkers, "hash-workers", -1, "integer value for number of goroutines computing hashes. Setting this value to zero means to spawn a goroutine for each tree")
    flag.IntVar(&dataWorkers, "data-workers", -1, "integer value for number of goroutines inputing datainto the shared map structure")
    flag.IntVar(&compWorkers, "comp-workers", -1, "integer value for number of goroutines running compairsons on the trees")
    //Input parsing is now parallel by default
    flag.IntVar(&inputWorkers, "input-workers", 1, "number of goroutines to use for creating trees")
    
    flag.BoolVar(&debug, "debug", false, "Enables printing of full output for debug purposes")

    var inputPath *string = flag.String("input", "", "string value for input path")
    
    flag.Parse()
    
    //fmt.Printf("Hash workers: %d dataWorkers: %d compWorkers: %d inputPath: %s input workers: %d\n", hashWorkers, dataWorkers, compWorkers, *inputPath, inputWorkers)
    fileBytes, err := ioutil.ReadFile(*inputPath)
    if err != nil {
        fmt.Println(err)
        flag.Usage()
        os.Exit(1)
    }
    
    var fileData string = string(fileBytes)
    fileBytes = nil //Hopefully the GC collects it

    //hashmap where key is the hash (int)and value is a list of the ids whose trees hash to this (vector/slice of ints)
    var hashToId map[int]*[]int = make(map[int]*[]int)
    var seArr []IntPair = make([]IntPair, 0)

    var start int = 0
    var lineCounter int = 0
    var fileDataLen = len(fileData)
    for start < fileDataLen {
        
        var end int = strings.IndexRune(fileData[start:fileDataLen], '\n')
        if end == -1 {
            break;
        }
        
        end += start

        seArr = append(seArr, IntPair{first: start, second: end})
       
        start = end + 1
        lineCounter++

    }

    var nTrees = len(seArr)
    var treeStore []BinarySearchTree = make([]BinarySearchTree, nTrees)
    var createStart time.Time = time.Now()

    if (inputWorkers == 1) || (nTrees < inputWorkers) {
        inputWorkers = nTrees
    }
    
    if inputWorkers == 0 {
        
        for i := 0; i < nTrees; i++ {
            treeStore[i].CreateTreeFromSlice(fileData[seArr[i].first:seArr[i].second])
        }

    } else {
        
        var barrier sync.WaitGroup
        barrier.Add(inputWorkers)

        for i := 0; i < inputWorkers; i++ {
            
            go func(index int){
                for index < nTrees{
                                                            
                    treeStore[index].CreateTreeFromSlice(fileData[seArr[index].first:seArr[index].second])
                    index += inputWorkers
                }
                barrier.Done()
            }(i)
        }

        barrier.Wait()
    }

    var createEnd time.Time = time.Now()
    var createDur float64 = (createEnd.Sub(createStart)).Seconds()

    //Main thread leaves after so it should be safe to release here
    fileData = ""
    seArr = nil
    
    var traversalStore []*[]int = make([]*[]int, nTrees)
    
    //If hash workers is 0, that will signal spawning a goroutine for each tree
    if hashWorkers == 0 {
        hashWorkers = nTrees
    }

    //Case where only the hash time matters - ask if traversal store applies here or if it can be removed
    if dataWorkers == -1 {
        var hashOnlyStart time.Time = time.Now()
        if hashWorkers == 1 {
            //Can the compiler optimize out this work?

            for i := 0; i < nTrees; i++ {
                var traversal []int = make([]int, treeStore[i].size)
                traversalStore[i] = &traversal
                _ = treeStore[i].OrderAndHash(&traversal)    
            }

        } else {

            var barrier sync.WaitGroup
            barrier.Add(hashWorkers)

            for i := 0; i < hashWorkers; i++ {
                
                go func(index int){

                    for index < nTrees{

                        var traversal []int = make([]int, treeStore[index].size)
                        traversalStore[index] = &traversal
                        _ = treeStore[index].OrderAndHash(&traversal)
                        
                        index += hashWorkers
                    }
                    barrier.Done()
                }(i)
            }

            barrier.Wait()
        }
        
        var hashOnlyEnd time.Time = time.Now()
        var hashOnlyTime float64 = (hashOnlyEnd.Sub(hashOnlyStart)).Seconds()
        var programEndTime time.Time = time.Now()
        var programDur float64 = (programEndTime.Sub(programStartTime)).Seconds()
        fmt.Printf("hashTime: %.12f\ne2eTime: %.12f\n", hashOnlyTime, programDur)
        os.Exit(0)
    }

    //Section 1: Compute tree hashes
    var hashStart time.Time = time.Now()
    var hashEnd time.Time
    var groupEnd time.Time

    if hashWorkers == 1 { //Sequential
        for i := 0; i < nTrees; i++ {
            var traversal []int = make([]int, treeStore[i].size)
            traversalStore[i] = &traversal
            var hash int = treeStore[i].OrderAndHash(&traversal)
            
            if i == (nTrees - 1) {
                //Save time when last hash finishes
                hashEnd = time.Now()
            }

            _, ok := hashToId[hash]
            if !ok {
                var nList []int = make([]int, 0)
                hashToId[hash] = &nList
            }
            *hashToId[hash] = append(*hashToId[hash], i)

            if i == (nTrees - 1) {
                groupEnd = time.Now()
            }
        }
        

    } else if dataWorkers == 1 { //Channel
        
        var barrier sync.WaitGroup
        //Extra sync required from goroutine updating the map
        barrier.Add(hashWorkers + 1)
        //Can we buffer this?
        var channel chan IntPair = make(chan IntPair, nTrees)

        //Goroutine to update map
        go func(){
            var iters int = 0
            for iters < nTrees{
                iters++
                //first is hash, second is BST id/index
                data := <-channel

                _, ok := hashToId[data.first]
                if !ok {
                    var nList []int = make([]int, 0)
                    hashToId[data.first] = &nList
                }
                *hashToId[data.first] = append(*hashToId[data.first], data.second)
            }
            
            groupEnd = time.Now() //All hashes should be stored at this point
            barrier.Done()
        }() 
        
        //Tracks number of trees that need to be hashed
        var numHashed int64
        atomic.StoreInt64(&numHashed, int64(nTrees))

        //Creating worker goroutines
        //If more hashWorkers are requested then there are trees, the extra goroutines
        //will just do nothing
        for i := 0; i < hashWorkers; i++ {
            
            go func(index int){

                for index < nTrees{

                    var traversal []int = make([]int, treeStore[index].size)
                    traversalStore[index] = &traversal
                    var hash int = treeStore[index].OrderAndHash(&traversal)

                    var hc int64 = atomic.AddInt64(&numHashed, -1)
                    if hc == 0 {
                        hashEnd = time.Now()
                    }

                    channel <- IntPair{hash, index}
                    
                    index += hashWorkers
                }
                barrier.Done()
            }(i)
        }

        barrier.Wait()

    } else if hashWorkers == dataWorkers { //Mutex
        
        var barrier sync.WaitGroup
        barrier.Add(hashWorkers)
        var mapMutex sync.Mutex
        
        var numHashed int64
        var numStored int64
        atomic.StoreInt64(&numHashed, int64(nTrees))
        atomic.StoreInt64(&numStored, int64(nTrees))
        
        for i := 0; i < hashWorkers; i++ {
            
            go func(index int){

                for index < nTrees{

                    var traversal []int = make([]int, treeStore[index].size)
                    traversalStore[index] = &traversal
                    var hash int = treeStore[index].OrderAndHash(&traversal)

                    var hc int64 = atomic.AddInt64(&numHashed, -1)
                    if hc == 0 {
                        hashEnd = time.Now()
                    }

                    var sc int64 = atomic.AddInt64(&numStored, -1)

                    mapMutex.Lock()

                    _, ok := hashToId[hash]
                    if !ok {
                        var nList []int = make([]int, 0)
                        hashToId[hash] = &nList
                    }
                    *hashToId[hash] = append(*hashToId[hash], index)
                    
                    mapMutex.Unlock()
                    
                    if sc == 0 {
                        groupEnd = time.Now()
                    }

                    index += hashWorkers
                }
                barrier.Done()
            }(i)
        }

        barrier.Wait()

    } else { //Multiple goroutines hashing and multiple goroutines populating map with fine
        //grained locking. This is extra credit
        //TODO: consider swapping the atomic stuff here to be more inline with newer Go versions
        
        var hashLocks map[int]*sync.Mutex = make(map[int]*sync.Mutex)
        //Should protect the map of locs and the map of hashes to lists
        var mapModLock sync.RWMutex

        var numHashed int64
        var numStored int64
        atomic.StoreInt64(&numHashed, int64(nTrees))
        atomic.StoreInt64(&numStored, int64(nTrees))
        
        //Going for the implementation where the hash workers are also the data workers
        //This does throttle the hash workers due to the semaphore but avoids overhead of creating
        //the extra data workers and their corresponding channels
        var sem chan struct{} = make(chan struct{}, dataWorkers)

        var barrier sync.WaitGroup
        barrier.Add(hashWorkers)

        for i := 0; i < hashWorkers; i++ {
            
            go func(index int){

                for index < nTrees{
                    
                    var traversal []int = make([]int, treeStore[index].size)
                    traversalStore[index] = &traversal
                    var hash int = treeStore[index].OrderAndHash(&traversal)

                    var hc int64 = atomic.AddInt64(&numHashed, -1)
                    if hc == 0 {
                        hashEnd = time.Now()
                    }
                    
                    var sc int64 = atomic.AddInt64(&numStored, -1)
                    //Data worker stage
                    sem <- struct{}{}
                     
                    mapModLock.RLock()
                    _, ok := hashLocks[hash]
                    mapModLock.RUnlock()

                    if !ok { 

                        //If no lock exists, want the write lock so you can add it
                        mapModLock.Lock()
                        //Have RW access so can safely check if another thread beat you to adding the lock
                        //Im very good at naming things
                        _, ok2 := hashLocks[hash]
                        if !ok2 {
                            
                            var spotLock sync.Mutex
                            hashLocks[hash] = &spotLock
                            var nList []int = make([]int, 0)
                            hashToId[hash] = &nList
                        }

                         mapModLock.Unlock()
                    }

                    mapModLock.RLock()
                    //Grab pointers to the locks and the list. These should not change even if the map is resized
                    var spotLockPtr *sync.Mutex = hashLocks[hash]
                    var hashListPtr *[]int = hashToId[hash]
                    
                    mapModLock.RUnlock()
                    
                    (*spotLockPtr).Lock()
                    *hashListPtr = append(*hashListPtr, index)
                    (*spotLockPtr).Unlock()

                    <-sem
                    
                    if sc == 0 {
                        groupEnd = time.Now()
                    }

                    index += hashWorkers
                }

                barrier.Done()
            }(i)
        }

        
        barrier.Wait()        
    }

    var hashDur float64 = (hashEnd.Sub(hashStart)).Seconds()
    var groupDur float64 = (groupEnd.Sub(hashStart)).Seconds()
    
    if compWorkers == -1 {

        fmt.Printf("hashGroupTime: %.12f\n", groupDur)
        
        for hash, slc := range hashToId {

            if len(*slc) > 1 {
                sort.Slice(*slc, func(first int, second int) bool {
                    return (*slc)[first] < (*slc)[second]
                })      
                fmt.Printf("%d: ", hash)
                for _, val := range *slc {
                    fmt.Printf("%d ", val)
                }
                fmt.Println()
            }
        }
        
        var programEndTime time.Time = time.Now()
        var programDur float64 = (programEndTime.Sub(programStartTime)).Seconds()
        fmt.Printf("e2eTime: %.12f\n", programDur)
        os.Exit(0)
    }

    //Should default initialize everything to false on its own
    var adjacency []bool = make([]bool, nTrees * nTrees)
    getIndex := func(row int, col int) int {
        return (row * nTrees) + col
    }

    var compStart time.Time = time.Now() 
    
    //Spawn goroutine for each comparison
    if compWorkers == 0 {
        
        var barrier sync.WaitGroup

        for _, hashGroup := range hashToId {

            var groupLen = len(*hashGroup)
            if groupLen == 1 {
                adjacency[getIndex((*hashGroup)[0], (*hashGroup)[0])] = true
            } else {
                
                for i := 0; i < groupLen; i++ {
                    for j := 0; j < groupLen; j++ {
                        var firstTree int = (*hashGroup)[i]
                        var secondTree int = (*hashGroup)[j]
                        //If the are same or another thread has done this comparison don't need
                        //to compare
                        if (firstTree == secondTree)  || adjacency[getIndex(secondTree, firstTree)]{
                            adjacency[getIndex(firstTree, secondTree)] = true
                        } else {
                            
                            barrier.Add(1)
                            go func(tree1 int, tree2 int) {
                                
                                adjacency[getIndex(tree1, tree2)] = compTraversals(traversalStore[tree1], traversalStore[tree2])
                                barrier.Done()
                            }(firstTree, secondTree)

                        }
                    }
                }

            }
        }

        barrier.Wait()

    } else if compWorkers == 1 { //Sequential

        for _, hashGroup := range hashToId {

            var groupLen = len(*hashGroup)
            if groupLen == 1 {
                adjacency[getIndex((*hashGroup)[0], (*hashGroup)[0])] = true
            } else {
                
                for i := 0; i < groupLen; i++ {
                    for j := 0; j < groupLen; j++ {
                        var firstTree int = (*hashGroup)[i]
                        var secondTree int = (*hashGroup)[j]
                        //If the are same or another thread has done this comparison don't need
                        //to compare
                        if (firstTree == secondTree)  || adjacency[getIndex(secondTree, firstTree)]{
                            adjacency[getIndex(firstTree, secondTree)] = true
                        } else {
                            adjacency[getIndex(firstTree, secondTree)] = compTraversals(traversalStore[firstTree], traversalStore[secondTree])
                        }
                    }
                }

            }
        }

    } else { //Custom buffer
        var buffer *CBuffer = CBufferInit(compWorkers)
        var barrier sync.WaitGroup
        barrier.Add(compWorkers)
       
        //Spawning comp worker threads
        for i := 0; i < compWorkers; i++ {
            go func(){
                for {
                    var compPair IntPair = (*buffer).Get()

                    //Signal that the goroutine should shut down
                    if (compPair.first == -1) && (compPair.second == -1){
                        break
                    }
                    
                    //Acceptable data race here, just a minor optimization missed
                    adjacency[getIndex(compPair.first, compPair.second)] = compTraversals(traversalStore[compPair.first], traversalStore[compPair.second])

                }

                barrier.Done()
            }()
        }

        
        for _, hashGroup := range hashToId {

            var groupLen = len(*hashGroup)
            if groupLen == 1 {
                adjacency[getIndex((*hashGroup)[0], (*hashGroup)[0])] = true
            } else {
                
                for i := 0; i < groupLen; i++ {
                    for j := 0; j < groupLen; j++ {
                        var firstTree int = (*hashGroup)[i]
                        var secondTree int = (*hashGroup)[j]
                        //If the are same or another thread has done this comparison don't need
                        //to compare
                        if (firstTree == secondTree)  || adjacency[getIndex(secondTree, firstTree)]{
                            adjacency[getIndex(firstTree, secondTree)] = true
                        } else {
                            (*buffer).Put(IntPair{first: firstTree, second: secondTree})
                        }
                    }
                }

            }
        }

        for i := 0; i < compWorkers; i++ {
            (*buffer).Put(IntPair{first: -1, second: -1})
        }

        barrier.Wait()
    }

    var compEnd time.Time = time.Now()
    var compDur float64 = (compEnd.Sub(compStart)).Seconds()
    
    //Print everything if debug is enabled
    if debug {
        
        fmt.Printf("Tree creation time: %.12f\n", createDur)
        fmt.Printf("hashTime: %.12f\nhashGroupTime: %.12f\n", hashDur, groupDur)
        
        for hash, slc := range hashToId {

            sort.Slice(*slc, func(first int, second int) bool {
                return (*slc)[first] < (*slc)[second]
            })      
            fmt.Printf("%d: ", hash)
            for _, val := range *slc {
                fmt.Printf("%d ", val)
            }
            fmt.Println()
        }
        
    }

    fmt.Printf("compareTreeTime: %.12f\n", compDur)

    ///*
    var groupId int = 0
    //No sets :(
    var seenMap map[int]bool = make(map[int]bool)
    
    for _, hashGroup := range hashToId {
        var groupLen int = len(*hashGroup)
        if groupLen > 1 {
            
            for i := 0; i < groupLen; i++ {
                var treeId int = (*hashGroup)[i]
                //If element has already been added to another group
                _, ok := seenMap[treeId]
                if ok {
                    continue
                }

                fmt.Printf("group %d:", groupId)
                groupId++
                var startInd int = getIndex(treeId, 0)
                
                for j := 0; j < nTrees; j++ {
                    if adjacency[startInd + j] {
                        fmt.Printf(" %d", j)
                        //This should get the current tree id too
                        seenMap[j] = true
                    }
                }
                fmt.Printf("\n")

            }

        }
    }
    //*/

    var programEndTime time.Time = time.Now()
    var programDur float64 = (programEndTime.Sub(programStartTime)).Seconds()
    fmt.Printf("e2eTime: %.12f\n", programDur)

}
