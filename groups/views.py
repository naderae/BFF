from django.shortcuts import render, redirect, get_object_or_404
from django.contrib.auth.decorators import login_required
from .models import Group
from posts.models import Post
from comments.forms import CommentForm
# Create your views here.
@login_required
def create(request):
    if request.method == 'POST':
        if request.POST['name'] and request.POST['description'] and request.FILES['image']:
            group = Group()
            group.name = request.POST['name']
            group.description = request.POST['description']
            group.image = request.FILES['image']
            group.save()
            return redirect('groups:index')
        else:
            return render(request, 'groups/create.html', {'error':'All fields must be filled out.'})
    else:
        return render(request, 'groups/create.html')

def index(request):
    groups = Group.objects
    return render(request, 'groups/index.html', {'groups':groups} )

def detail(request, group_id):
    group = get_object_or_404(Group, pk= group_id)
    posts = Post.objects.filter(group__id = group_id)

    # if post_id != None:
    # post = Post.objects.first
    # else:
    #     post = None

    # form = CommentForm(request.POST)

    return render(request, 'groups/detail.html', {'group': group, 'posts':posts})

def join(request, group_id):
    group = get_object_or_404(Group, pk= group_id)
    if request.method == 'POST':
        group.members.add(request.user)
        # group.save()
        return redirect('/groups/' + str(group_id) )
    else:
        return render(request, '/groups/detail.html', {'group': group})

def leave(request, group_id):
    group = get_object_or_404(Group, pk= group_id)
    print('the leave function got called')
    if request.method == 'POST':
                group.members.remove(request.user)
                # group.save()
                return redirect('/groups/' + str(group_id) )
    else:
            return render(request, '/groups/index.html')
        # return render(request, '/groups/detail.html', {'group': group})
